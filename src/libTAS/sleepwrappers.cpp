/*
    Copyright 2015-2016 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sleepwrappers.h"
#include "logging.h"
#include "threadwrappers.h"
#include "DeterministicTimer.h"
#include "backtrace.h"
#include "GlobalState.h"
#include "hook.h"

namespace libtas {

DEFINE_ORIG_POINTER(nanosleep);

/* Override */ void SDL_Delay(unsigned int sleep)
{
    LINK_NAMESPACE(nanosleep, nullptr);

    struct timespec ts;
    ts.tv_sec = sleep / 1000;
    ts.tv_nsec = (sleep % 1000) * 1000000;

    if (GlobalState::isNative()) {
        orig::nanosleep(&ts, NULL);
        return;
    }

    bool mainT = isMainThread();
    debuglog(LCF_SDL | LCF_SLEEP | (mainT?LCF_NONE:LCF_FREQUENT), __func__, " call - sleep for ", sleep, " ms.");

    /* If the function was called from the main thread, transfer the wait to
     * the timer and do not actually wait.
     */
    if (sleep && mainT) {
        detTimer.addDelay(ts);
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }

    orig::nanosleep(&ts, NULL);
}

/* Override */ int usleep(useconds_t usec)
{
    LINK_NAMESPACE(nanosleep, nullptr);

    struct timespec ts;
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;

    if (GlobalState::isNative())
        return orig::nanosleep(&ts, NULL);

    bool mainT = isMainThread();
    debuglog(LCF_SLEEP | (mainT?LCF_NONE:LCF_FREQUENT), __func__, " call - sleep for ", usec, " us.");

    /* If the function was called from the main thread, transfer the wait to
     * the timer and do not actually wait.
     */
    if (usec && mainT) {
        detTimer.addDelay(ts);
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }

    orig::nanosleep(&ts, NULL);
    return 0;
}

/* Override */ int nanosleep (const struct timespec *requested_time, struct timespec *remaining)
{
    LINK_NAMESPACE(nanosleep, nullptr);

    if (GlobalState::isNative()) {
        return orig::nanosleep(requested_time, remaining);
    }

    bool mainT = isMainThread();
    debuglog(LCF_SLEEP | (mainT?LCF_NONE:LCF_FREQUENT), __func__, " call - sleep for ", requested_time->tv_sec * 1000000000 + requested_time->tv_nsec, " nsec");

    /* If the function was called from the main thread, transfer the wait to
     * the timer and do not actually wait.
     */
    if (mainT && (requested_time->tv_sec || requested_time->tv_nsec)) {
        detTimer.addDelay(*requested_time);
        struct timespec owntime = {0, 0};
        return orig::nanosleep(&owntime, remaining);
    }

    return orig::nanosleep(requested_time, remaining);
}

/* Override */int clock_nanosleep (clockid_t clock_id, int flags,
			    const struct timespec *req,
			    struct timespec *rem)
{
    bool mainT = isMainThread();
    TimeHolder sleeptime;
    sleeptime = *req;
    if (flags == 0) {
        /* time is relative */
    }
    else {
        /* time is absolute */
        struct timespec curtime = detTimer.getTicks();
        sleeptime -= curtime;
    }

    debuglog(LCF_SLEEP | (mainT?LCF_NONE:LCF_FREQUENT), __func__, " call - sleep for ", sleeptime.tv_sec * 1000000000 + sleeptime.tv_nsec, " nsec");

    /* If the function was called from the main thread
     * and we are not in the native state,
     * transfer the wait to the timer and
     * do not actually wait
     */
    LINK_NAMESPACE(nanosleep, nullptr);
    if (mainT && !GlobalState::isNative()) {
        detTimer.addDelay(sleeptime);
        struct timespec owntime = {0, 0};
        return orig::nanosleep(&owntime, rem);
    }

    return orig::nanosleep(&sleeptime, rem);
}

}
