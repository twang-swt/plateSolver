#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>

#include "IPlateSolver.h"

namespace SW {

// PlateSolver that uses StellarSolver.
//
// # Blocking note
// StellarSolver relies on Qt signal/slots, which normally
// needs Qt event loop (which needs QCoreApplication instance) to work.
// If QCoreApplication is not available, solveAsync() becomes blocking
// and solveAsync()'s handler is called before solveAsync() returns.
class PlateSolver : public IPlateSolver {
public:
    struct SolverScale {
        SolverScale() : min(0), max(0) {}
        SolverScale(double min, double max) : min(min), max(max) {}

        //Unit in ARCSEC_PER_PIX
        double min, max;
    };

    class CancelFlag {
    public:
        //For reader: check if cancelled
        bool isCancelled() {
            std::lock_guard<std::mutex> lk(_mxEvent);
            return _isCancelled.load();
        }

        //For writer: set cancel flag
        void cancel() {
            std::lock_guard<std::mutex> lk(_mxEvent);
            _isCancelled.store(true);
        }

    private:
        std::mutex _mxEvent;
        std::atomic_bool _isCancelled{false};
    };

public:
    //`fitsPath` is directory containing .fits files
    PlateSolver(std::string_view fitsPath, bool enableLog);
    ~PlateSolver() override;

    virtual void solveAsync(
        std::string_view filePath,
        const std::optional<AstroCoord> &hintRADec,
        bool usePrevInfo,
        ImagePos solvePoint,
        Handler handler
    ) override;
    virtual void cancel() override;

    //-----------------------------------------
    //other interface
    //-----------------------------------------
    //Directly set prevInfo
    void setPrevInfo(std::optional<SolverScale> value) { _prevInfo = value; }

private:
    struct Impl;
    std::unique_ptr<Impl> _m;

    //There's a canceller for each task instance.
    //This is for the most recent task.
    //This does not clear when task ends.
    std::shared_ptr<CancelFlag> _canceller;

    //Info from last successful solve
    std::optional<SolverScale> _prevInfo;
};

} //namespace SW
