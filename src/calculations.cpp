#include "calculations.h"

#include <sstream>
#include <cstring>

using namespace std;

void orderDispls(list<shared_ptr<Displ>> &displs, const list<string> &order) {

    // stack all the preferred, available displays
    list<shared_ptr<Displ>> preferredDispls;
    for (const auto &name : order) {
        for (const auto &displ : displs) {
            if (strcasecmp(name.c_str(), displ->name.c_str()) == 0) {
                preferredDispls.push_front(displ);
            }
        }
    }

    // move preferred to the front
    for (const auto &preferredDispl : preferredDispls) {
        displs.remove(preferredDispl);
        displs.push_front(preferredDispl);
    }
}

void activateDispls(std::list<shared_ptr<Displ>> &displs, const string &primary, const Monitors &monitors) {
    for (const auto &displ : displs) {

        // don't display any monitors that shouldn't
        if (monitors.shouldDisableDisplay(displ->name))
            continue;

        // only activate currently active or connected displays
        if (displ->state != Displ::active && displ->state != Displ::connected)
            continue;

        // mark active
        displ->desiredActive(true);

        // default first to primary
        if (!Displ::desiredPrimary)
            Displ::desiredPrimary = displ;

        // user selected primary
        if (!primary.empty() && strcasecmp(primary.c_str(), displ->name.c_str()) == 0)
            Displ::desiredPrimary = displ;
    }
}

void ltrDispls(list<shared_ptr<Displ>> &displs) {
    int xpos = 0;
    int ypos = 0;
    for (const auto &displ : displs) {

        if (displ->desiredActive()) {

            // set the desired mode to optimal
            displ->desiredMode(displ->optimalMode);

            // position the screen
            displ->desiredPos = make_shared<Pos>(xpos, ypos);

            // next position
            xpos += displ->desiredMode()->width;
        }
    }
}

void mirrorDispls(list<shared_ptr<Displ>> &displs) {

    // find the first active display
    shared_ptr<Displ> firstDispl;
    for (const auto &displ : displs) {
        if (displ->desiredActive()) {
            firstDispl = displ;
            break;
        }
    }
    if (!firstDispl)
        return;

    // iterate through first active display's modes
    for (const auto &possibleMode : firstDispl->modes) {
        bool matched = true;

        // attempt to match mode to each active displ
        for (const auto &displ : displs) {
            if (!displ->desiredActive())
                continue;

            // reset failed matches
            shared_ptr<Mode> desiredMode;

            // match height and width only
            for (const auto &mode : displ->modes) {
                if (mode->width == possibleMode->width && mode->height == possibleMode->height) {

                    // set mode and pos
                    desiredMode = mode;
                    break;
                }
            }

            // match a mode for every display; root it at 0, 0
            matched = matched && desiredMode;
            if (matched) {
                displ->desiredMode(desiredMode);
                displ->desiredPos = make_shared<Pos>(0, 0);
                continue;
            }
        }

        // we've set desiredMode and desiredPos (zero) for all displays, all done
        if (matched)
            return;
    }

    // couldn't find a common mode, exit
    throw runtime_error("unable to find common width/height for mirror");
}

// TODO: document and test this; refactor needed
string calculateDpi(std::list<shared_ptr<Displ>> &displs) {
    stringstream verbose;
    if (!Displ::desiredPrimary) {
        verbose << "DPI defaulting to "
                << Displ::desiredDpi
                << "; no primary display has been set set";
    } else if (!Displ::desiredPrimary->edid) {
        verbose << "DPI defaulting to "
                << Displ::desiredDpi
                << "; EDID information not available for primary display "
                << Displ::desiredPrimary->name;
    } else if (!Displ::desiredPrimary->desiredMode()) {
        verbose << "DPI defaulting to "
                << Displ::desiredDpi
                << "; desiredMode not available for primary display "
                << Displ::desiredPrimary->name;
    } else {
        const long desiredDpi = Displ::desiredPrimary->edid->dpiForMode(Displ::desiredPrimary->desiredMode());
        if (desiredDpi == 0) {
            verbose << "DPI defaulting to "
                    << Displ::desiredDpi
                    << "; no display size EDID information available for "
                    << Displ::desiredPrimary->name;
        } else {
            Displ::desiredDpi = desiredDpi;
            verbose << "DPI "
                    << Displ::desiredDpi
                    << " for primary display "
                    << Displ::desiredPrimary->name;
        }
    }

    return verbose.str();
}

shared_ptr<Mode> generateOptimalMode(const list<shared_ptr<Mode>> &modes, const shared_ptr<Mode> &preferredMode) {
    shared_ptr<Mode> optimalMode;

    // default optimal mode is empty
    if (!modes.empty()) {

        // use highest mode for optimal
        optimalMode = modes.front();

        // override with highest refresh of preferred
        if (preferredMode)
            for (auto &mode : modes)
                if (mode->width == preferredMode->width && mode->height == preferredMode->height) {
                    optimalMode = mode;
                    break;
                }
    }

    return optimalMode;
}