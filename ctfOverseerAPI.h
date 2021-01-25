#ifndef CTFOVERSEER_API_H
#define CTFOVERSEER_API_H

#include <functional>
#include <utility>

#include "bzfsAPI.h"

/**
 * The pairing of bz_eTeamType values used. The first value of the pair is the team that is grabbing or capturing the
 * enemy flag. The second value is the team whose flag is being stolen or captured.
 *
 * @since 1.1.2
 */
typedef std::pair<bz_eTeamType, bz_eTeamType> TeamPair;

/**
 * Function signature used for the "on capture" callback that's called by CTF Overseer.
 *
 * The "on capture" event triggered by CTF Overseer is different than BZFlag's official bz_eCaptureEvent in that in this
 * callback, information regarding self-capture, unfairness, and disallowing flag captures.
 *
 * @arg playerID - The playerID of the player who triggered the capping event
 * @arg isUnfair - Whether or not the capture event was considered "fair"
 * @arg wasDisallowed - Whether or not the capture event was disallowed because it was unfair or a self-capture
 * @arg isSelfCap - Whether or not the capture event was triggered by a self-capture
 *
 * @since 1.1.2
 */
typedef std::function<void(int playerID, bool isUnfair, bool wasDisallowed, bool isSelfCap)> OnCaptureEventCallbackV1;

#endif
