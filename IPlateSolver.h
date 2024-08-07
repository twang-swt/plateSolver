#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string_view>

namespace SW {

/**
Astrometry plate solver.
*/
class IPlateSolver {
public:
    struct AstroCoord {
        AstroCoord() : ra(0), dec(0) {}
        AstroCoord(double ra, double dec) : ra(ra), dec(dec) {}
        //angles in radian
        double ra, dec;
    };

    struct Result {
        std::optional<double> fieldWidth;
        std::optional<double> fieldHeight;
        //RDJ2000 in radian.
        std::optional<AstroCoord> rdJ2000;
        std::optional<double> pixelScale;
        std::optional<double> rotation;
        bool isCancelled = false;

        bool isValid() {
            return fieldWidth && fieldHeight && rdJ2000 && pixelScale && rotation && !isCancelled;
        }
    };

    using Handler = std::function<void(Result result)>;

    //Position {x, y} in image coordinates.
    //Range: eg 1800x4000 image is {0~1799, 0~3999}
    //Direction: eg {x=100, y=200} is 100 from top, 200 from left.
    using ImagePos = std::array<double, 2>;

public:
    virtual ~IPlateSolver() = default;

    //Start solving. Can start at any state.
    //- photo at `filePath`.
    //- find sky position of the image position `solvePoint`.
    //- `hintRADec` is approximate location the sky position.
    //- `usePrevInfo` indicate whether to use info from previous solve (if any).
    //- `handler` will always be invoked once.
    virtual void solveAsync(
        std::string_view filePath,
        const std::optional<AstroCoord> &hintRADec,
        bool usePrevInfo,
        ImagePos solvePoint,
        Handler handler
    ) = 0;

    //Cancel async. This does not block caller.
    virtual void cancel() = 0;
};

} //namespace SW
