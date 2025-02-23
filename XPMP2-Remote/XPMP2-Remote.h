/// @file       XPMP2-Remote.h
/// @brief      XPMP2 Remote Client: Displays aircraft served from other
///             XPMP2-based plugins in the network
/// @details    This plugin is intended to be used in a multi-computer simulator
///             setup, usually in the PCs used for external visuals.\n
///             The typical setup would be:
///             - There is a multi-computer setup of one X-Plane Master PC,
///               which also runs one or more XPMP2-based plugins like LiveTraffic,
///               which create additional traffic, let's call them "traffic master".
///             - Other PCs serve to compute additional external visuals.
///               For them to be able to show the very same additional traffic
///               they run XPMP2 Remote Client, which will display a copy
///               of the traffic generated by the XPMP2-based plugin on the master.
///
///             Technically, this works as follows:
///             - The "traffic masters" will first _listen_
///               on the network if anyone is interested in their data.
///             - The XPMP2 Remote Client will first send a "beacon of interest"
///               message to the network.
///             - This messages tells the master plugins to start feeding their data.
///             - All communication is UDP multicast on the same multicast
///               group that X-Plane uses, too: 239.255.1.1, but on a different
///               port: 49788
///             - This generic way allows for many different setups:
///               - While the above might be typical, it is not of interest if the
///                 "traffic master" is running on the X-Plane Master or any
///                 other X-Plane instance in the network, for example to
///                 better balance the load.
///               - It could even be an X-Plane PC not included in the
///                 External Visuals setup, like for example in a Networked
///                 Multiplayer setup.
///               - Multiple XPMP2-based traffic masters can be active, and
///                 they can even run on different PCs. The one XPMP2 Remote Client
///                 per PC will still collect all traffic.
///               - If several traffic masters run on different PCs, then _all_
///                 PCs, including the ones running one of the traffic masters,
///                 will need to run the XPMP2 Remote Client, so that they
///                 pick up the traffic generated on the _other_ traffic masters.
///
/// @see        For multi-computer setup of external visual:
///             https://x-plane.com/manuals/desktop/#networkingmultiplecomputersformultipledisplays
/// @see        For Networked Multiplayer:
///             https://x-plane.com/manuals/desktop/#networkedmultiplayer
/// @author     Birger Hoppe
/// @copyright  (c) 2020 Birger Hoppe
/// @copyright  Permission is hereby granted, free of charge, to any person obtaining a
///             copy of this software and associated documentation files (the "Software"),
///             to deal in the Software without restriction, including without limitation
///             the rights to use, copy, modify, merge, publish, distribute, sublicense,
///             and/or sell copies of the Software, and to permit persons to whom the
///             Software is furnished to do so, subject to the following conditions:\n
///             The above copyright notice and this permission notice shall be included in
///             all copies or substantial portions of the Software.\n
///             THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
///             IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///             FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
///             AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
///             LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
///             OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
///             THE SOFTWARE.

#pragma once

// Standard C headers
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>

// Standard C++ headers
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <algorithm>

// X-Plane SDK
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"
#include "XPLMPlugin.h"
#include "XPLMMenus.h"
#include "XPLMProcessing.h"

// Include XPMP2 headers
#include "XPMPAircraft.h"
#include "XPMPMultiplayer.h"
#include "XPMPRemote.h"

// Include XPMP2-Remote headers
#include "Utilities.h"
#include "Client.h"

// Windows: I prefer the proper SDK variants of min and max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

//
// MARK: Constants
//

constexpr const char* REMOTE_CLIENT_NAME    =  "XPMP2 Remote Client";   ///< Plugin name
constexpr const char* REMOTE_CLIENT_LOG     =  "XPMP2_RC";              ///< ID used in own log entries
constexpr const char* REMOTE_CLIENT_LOG2    =  "RC";                    ///< Short ID used in XPMP2 log entries
constexpr float REMOTE_CLIENT_VER           = 1.10f;

//
// MARK: Globals
//

/// Holds all global variables
struct XPMP2RCGlobals {
    
    /// Config values reconciled from sending plugins
    XPMP2::RemoteMsgSettingsTy mergedS;

    /// The global map of all sending plugins we've ever heard of
    mapSenderTy gmapSender;
    
    /// Latest timestamp read from network_time_sec
    float now = 0.0f;
    
    ///< id of X-Plane's thread (when it is OK to use XP API calls)
    std::thread::id xpThread;
    /// Is this thread XP's main thread?
    bool IsXPThread() const { return std::this_thread::get_id() == xpThread; }
};

/// the one and only instance of XPMP2RCGlobals
extern XPMP2RCGlobals rcGlob;
