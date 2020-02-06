/* Icinga 2 | (c) 2012 Icinga GmbH | GPLv2+ */

#include "icinga/notificationresult.hpp"
#include "icinga/notificationresult-ti.cpp"
#include "base/scriptglobal.hpp"

using namespace icinga;

REGISTER_TYPE(NotificationResult);

double NotificationResult::CalculateExecutionTime() const
{
	return GetExecutionEnd() - GetExecutionStart();
}
