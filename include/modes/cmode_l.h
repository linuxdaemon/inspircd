#include "mode.h"

class InspIRCd;

/** Channel mode +l
 */
class ModeChannelLimit : public ModeHandler
{
 public:
	ModeChannelLimit(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter);
	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel);
};
