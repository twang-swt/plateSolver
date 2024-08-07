#include "PlateSolver.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <optional>

namespace {
constexpr double radToDeg(double rad) {
    return rad / 3.14159265358979323846264338327950288 * 180.;
}
} //namespace

int main(int argc, char *argv[]) {
    using namespace SW;

#ifndef Q_OS_ANDROID
    QCoreApplication app(argc, argv);
#endif
    QCommandLineParser parser;
    parser.setApplicationDescription("Plate Solver");
    parser.addHelpOption();

    QCommandLineOption image_option(
        QStringList{"image", "i"}, "solver image.", "image"
    );
    parser.addOption(image_option);

    QCommandLineOption fits_option(
        QStringList{"fits", "f"}, "fits file path.(option)", "fits"
    );
    if (QCoreApplication::instance()) {
        fits_option.setDefaultValue(
            QCoreApplication::applicationDirPath() + QDir::separator() + "fits"
        );
    }
    parser.addOption(fits_option);

    QCommandLineOption scale_option(
        QStringList{"scale", "s"}, "scale min,max deg.(option)", "scale"
    );
    parser.addOption(scale_option);

    QCommandLineOption position_option(
        QStringList{"position", "p"}, "position ra,dec.(option)", "position"
    );
    parser.addOption(position_option);

    QCommandLineOption solver_position_option(
        QStringList{"solver_position", "S"},
        "solver position x,y.(option)",
        "solver_position"
    );
    solver_position_option.setDefaultValue("0,0");
    parser.addOption(solver_position_option);

    QCommandLineOption debug_option(
        QStringList{"debug", "d"}, "enable debug.(option)"
    );
    parser.addOption(debug_option);

    QStringList qargs;
    for (int i = 0; i < argc; i++) {
        qargs << argv[i];
    }
    parser.process(qargs);

    if (parser.isSet(image_option)) {
        QString solver_image = parser.value(image_option);
        QString fits_path = parser.value(fits_option);
        std::optional<QString> scale_min_max =
            parser.isSet(scale_option)
                ? std::optional<QString>{parser.value(scale_option)}
                : std::nullopt;
        std::optional<QString> position_ra_dec =
            parser.isSet(position_option)
                ? std::optional<QString>{parser.value(position_option)}
                : std::nullopt;
        QString solver_position = parser.value(solver_position_option);
        bool debug = parser.isSet(debug_option);

        std::optional<PlateSolver::SolverScale> solverScale = std::nullopt;
        std::optional<SW::IPlateSolver::AstroCoord> solverPosition =
            std::nullopt;
        SW::IPlateSolver::ImagePos solvePoint = {
            solver_position.split(",")[0].toDouble(),
            solver_position.split(",")[1].toDouble()
        };
        if (scale_min_max) {
            QStringList min_max = scale_min_max->split(",");
            double min = min_max[0].toDouble();
            double max = min_max[1].toDouble();
            solverScale = PlateSolver::SolverScale(min, max);
        }
        if (position_ra_dec) {
            QStringList ra_dec = position_ra_dec->split(",");
            double ra = ra_dec[0].toDouble();
            double dec = ra_dec[1].toDouble();
            solverPosition = std::optional<SW::IPlateSolver::AstroCoord>(
                SW::IPlateSolver::AstroCoord(ra, dec)
            );
        }
        auto plateSolver = std::make_shared<SW::PlateSolver>(
            fits_path.toUtf8().toStdString(), debug
        );
        plateSolver->setPrevInfo(solverScale);
        plateSolver->solveAsync(
            solver_image.toUtf8().toStdString(),
            solverPosition,
            true,
            solvePoint,
            [](SW::IPlateSolver::Result result) {
                if (result.isValid()) {
                    QJsonObject json_object;
                    json_object["field_width"] = result.fieldWidth.value();
                    json_object["field_height"] = result.fieldHeight.value();
                    json_object["ra_j2000"] = radToDeg(result.rdJ2000->ra);
                    json_object["dec_j2000"] = radToDeg(result.rdJ2000->dec);
                    json_object["pixel_scale"] = result.pixelScale.value();
                    json_object["rotation"] = result.rotation.value();

                    QJsonDocument document(json_object);
                    QString jsonString =
                        document.toJson(QJsonDocument::Indented);
                    QTextStream(stdout) << jsonString;
                } else {
                    QTextStream(stdout) << "{}";
                }
                exit(0);
            }
        );
    } else {
        parser.showHelp();
    }
#ifndef Q_OS_ANDROID
    return app.exec();
#endif
}
