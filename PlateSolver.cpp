#include "PlateSolver.h"

#include <chrono>
#include <future>
#include <thread>

#include <QCoreApplication>

#include "ssolverutils/fileio.h"
#include "stellarsolver/stellarsolver.h"

// #define LOG(...) qDebug("PlateSolver: " __VA_ARGS__)
#define LOG(...)

namespace SW {

namespace {

constexpr double degToRad(double deg) {
    return deg / 180. * 3.14159265358979323846264338327950288;
}

//Whether solveAsync() should be blocking
bool isBlocking() {
    bool ret = !QCoreApplication::instance();
    return ret;
}

struct Job {
    std::shared_ptr<StellarSolver> stellarSolver;
    fileio *imageLoader;
    std::string filePath;
    std::shared_ptr<PlateSolver::CancelFlag> cancellable;
    std::optional<PlateSolver::SolverScale> solverScale;
    std::optional<IPlateSolver::AstroCoord> hintRADec;
    IPlateSolver::ImagePos solvePoint;
    IPlateSolver::Handler handler;
    std::chrono::high_resolution_clock::time_point startTime;
};

void runJob_onStellerSolverThread(std::shared_ptr<Job> job) {
    LOG("start load image");

    //Note: File load can take 300 ms
    bool loadOk =
        job->imageLoader->loadImage(QString::fromStdString(job->filePath));
    if (!loadOk) {
        job->handler({});
        return;
    }

    StellarSolver &stellarSolver = *job->stellarSolver;

    //Load image.
    // Calling `getImageBuffer()` makes imageLoader give up ownership.
    // But StellarSolver does not take ownership of data.
    // So ensure `imageLoader` retains ownership (ie release previous when
    // loading next image).
    stellarSolver.loadNewImageBuffer(
        job->imageLoader->getStats(), job->imageLoader->getImageBuffer()
    );
    job->imageLoader->imageBufferTaken = false;

    //search scale must be set after image load
    if (job->solverScale) {
        stellarSolver.setProperty("UseScale", true);
        stellarSolver.setSearchScale(
            job->solverScale->min,
            job->solverScale->max,
            ScaleUnits::ARCSEC_PER_PIX
        );
    }

    //Search position must be set after image load
    /*
    if (job->hintRADec) {
        stellarSolver.setSearchPositionInDegrees(
            RadToDeg(job->hintRADec->RA), RadToDeg(job->hintRADec->Dec)
        );
    }
    */

    QObject::connect(
        &stellarSolver,
        &StellarSolver::ready,
        &stellarSolver,
        [job]() {
            LOG("StellarSolver::ready");
            auto msElapsed = int64_t(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - job->startTime
                )
                    .count()
            );
            (void)msElapsed;
            LOG("Elapsed ms: %lld", msElapsed);

            if (job->cancellable->isCancelled()) {
                IPlateSolver::Result result;
                result.isCancelled = true;
                job->handler(result);
                return;
            }

            StellarSolver &stellarSolver = *job->stellarSolver;

            bool solved =
                !stellarSolver.failed() && stellarSolver.solvingDone();
            if (!solved) {
                job->handler({});
                return;
            }

            QPointF wcsPixelPoint{
                std::get<0>(job->solvePoint),
                std::get<1>(job->solvePoint),
            };
            FITSImage::wcs_point wcsCoord;
            bool wcsOk = stellarSolver.pixelToWCS(wcsPixelPoint, wcsCoord);
            if (!wcsOk) {
                job->handler({});
                return;
            }
            FITSImage::Solution solution = stellarSolver.getSolution();

            IPlateSolver::Result result{
                solution.fieldWidth,
                solution.fieldHeight,
                IPlateSolver::AstroCoord{
                    degToRad(wcsCoord.ra),
                    degToRad(wcsCoord.dec),
                },
                solution.pixscale,
                solution.orientation
            };
            job->handler(result);
        },
        Qt::ConnectionType(
            Qt::SingleShotConnection |
            (isBlocking() ? Qt::DirectConnection : Qt::AutoConnection)
        )
    );

    LOG("start solving");
    stellarSolver.start();
}

} //namespace

//---------------------------------------------------------------------------
//PlateSolver
//---------------------------------------------------------------------------
struct PlateSolver::Impl {
    //This is a shared_ptr to allow PlateSolver to discard it while async task
    //is on going
    std::shared_ptr<StellarSolver> stellarSolver;

    //This has same lifetime as stellarSolver, because stellarSolver reads
    // data that this object owns.
    fileio *imageLoader = nullptr;
};

PlateSolver::PlateSolver(std::string_view fitsPath, bool enableLog)

{
    _m = std::unique_ptr<PlateSolver::Impl>(new PlateSolver::Impl());
    _m->stellarSolver = std::make_shared<StellarSolver>();
    _m->imageLoader = new fileio();
    _m->imageLoader->setParent(_m->stellarSolver.get());
    StellarSolver &stellarSolver = *_m->stellarSolver;

    //QObject with null .thread() cannot receive signal
    assert(stellarSolver.thread() != nullptr);

    //Refer to StellarSolverTester's mainwindow.cpp
    stellarSolver.setProperty("ProcessType", ProcessType::SOLVE);
    stellarSolver.setProperty(
        "ExtractorType", ExtractorType::EXTRACTOR_INTERNAL
    );
    stellarSolver.setProperty("SolverType", SolverType::SOLVER_STELLARSOLVER);

    //Based on StellarSolver::getBuiltInProfiles() "3-LargeScaleSolving"
    Parameters param;
    //--------------------------
    //Star Extractor
    //--------------------------
    param.convFilterType = SSolver::CONV_GAUSSIAN;
    param.fwhm = 4;
    //--------------------------
    //Star Filter
    //--------------------------
    param.maxEllipse = 1.5;
    param.initialKeep = 500;
    param.keepNum = 50;
    //--------------------------
    //Astrometry Config
    //--------------------------
    //Note: successful solving usually takes 0.5 sec on desktop.
    param.solverTimeLimit = 5;
    param.minwidth = 0.05;
    param.maxwidth = 60;
    param.autoDownsample = false;

    stellarSolver.setParameters(param);

    QString fitsDir = QString::fromUtf8(fitsPath.data(), fitsPath.size());
    stellarSolver.setIndexFolderPaths({fitsDir});
    stellarSolver.setIndexFilePaths(StellarSolver::getIndexFiles({fitsDir}));

    stellarSolver.clearSubFrame();

    //Default "UseScale". May be overwritten later
    stellarSolver.setProperty("UseScale", false);

    //GREEN is default in StellarSolverTester
    stellarSolver.setColorChannel(FITSImage::ColorChannel::GREEN);

    if (enableLog) {
        stellarSolver.setLogLevel(logging_level::LOG_ALL);
        stellarSolver.setSSLogLevel(SSolverLogLevel::LOG_VERBOSE);
        QObject::connect(
            &stellarSolver,
            &StellarSolver::logOutput,
            [](QString logText) { qDebug().noquote() << logText; }
        );
    }
}

PlateSolver::~PlateSolver() = default;

void PlateSolver::solveAsync(
    std::string_view filePath,
    const std::optional<AstroCoord> &hintRADec,
    bool usePrevInfo,
    ImagePos solvePoint,
    Handler handler
) {
    LOG("solveAsync enter");
    cancel();

    std::shared_ptr<CancelFlag> cancellable;
    {
        auto cc = std::make_shared<CancelFlag>();
        cancellable = cc;
        _canceller = cc;
    }

    std::shared_ptr<Job> job = std::make_shared<Job>();
    job->stellarSolver = _m->stellarSolver;
    job->imageLoader = _m->imageLoader;
    job->filePath = filePath;
    job->cancellable = cancellable;
    job->solverScale = usePrevInfo ? _prevInfo : std::nullopt;
    job->hintRADec = hintRADec;
    job->solvePoint = solvePoint;

    std::promise<void> p_handlerCalled;
    std::future<void> f_handlerCalled;
    if (isBlocking()) {
        f_handlerCalled = p_handlerCalled.get_future();
        job->handler = [inner_handler = std::move(handler),
                        &p_handlerCalled](Result result) {
            inner_handler(result);
            p_handlerCalled.set_value();
        };
    } else {
        job->handler = std::move(handler);
    }
    job->startTime = std::chrono::high_resolution_clock::now();

    //block caller
    if (isBlocking()) {
        runJob_onStellerSolverThread(job);
        f_handlerCalled.wait();
        return;
    }

    std::thread t([job]() {
        LOG("previous task check: start");
        StellarSolver &stellarSolver = *job->stellarSolver;

        //ensure previous job ended
        if (stellarSolver.isRunning()) {
            LOG("previous task check: waiting for end");
            stellarSolver.abortAndWait();

            //check if this (ie not previous) job is cancelled
            if (job->cancellable->isCancelled()) {
                LOG("previous task check: this is cancelled");
                IPlateSolver::Result result;
                result.isCancelled = true;
                job->handler(result);
                return;
            }
        }

        QMetaObject::invokeMethod(&stellarSolver, [job] {
            runJob_onStellerSolverThread(job);
        });
    });
    t.detach();
}

void PlateSolver::cancel() {
    if (_m->stellarSolver->isRunning()) {
        if (_canceller) {
            _canceller->cancel();
            _canceller = nullptr;
        }
        _m->stellarSolver->abort();
    }
}

} //namespace SW
