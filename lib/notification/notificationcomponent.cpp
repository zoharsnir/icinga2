/* Icinga 2 | (c) 2012 Icinga GmbH | GPLv2+ */

#include "notification/notificationcomponent.hpp"
#include "notification/notificationcomponent-ti.cpp"
#include "icinga/service.hpp"
#include "icinga/icingaapplication.hpp"
#include "base/configtype.hpp"
#include "base/objectlock.hpp"
#include "base/logger.hpp"
#include "base/utility.hpp"
#include "base/exception.hpp"
#include "base/statsfunction.hpp"
#include "remote/apilistener.hpp"

using namespace icinga;

REGISTER_TYPE(NotificationComponent);

REGISTER_STATSFUNCTION(NotificationComponent, &NotificationComponent::StatsFunc);

void NotificationComponent::StatsFunc(const Dictionary::Ptr& status, const Array::Ptr&)
{
	DictionaryData nodes;

	for (const NotificationComponent::Ptr& notification_component : ConfigType::GetObjectsByType<NotificationComponent>()) {
		nodes.emplace_back(notification_component->GetName(), 1); //add more stats
	}

	status->Set("notificationcomponent", new Dictionary(std::move(nodes)));
}

/**
 * Starts the component.
 */
void NotificationComponent::Start(bool runtimeCreated)
{
	ObjectImpl<NotificationComponent>::Start(runtimeCreated);

	Log(LogInformation, "NotificationComponent")
		<< "'" << GetName() << "' started.";

	Checkable::OnNotificationsRequested.connect(std::bind(&NotificationComponent::SendNotificationsHandler, this, _1,
		_2, _3, _4, _5));

	m_NotificationTimer = new Timer();
	m_NotificationTimer->SetInterval(5);
	m_NotificationTimer->OnTimerExpired.connect(std::bind(&NotificationComponent::NotificationTimerHandler, this));
	m_NotificationTimer->Start();
}

void NotificationComponent::Stop(bool runtimeRemoved)
{
	Log(LogInformation, "NotificationComponent")
		<< "'" << GetName() << "' stopped.";

	ObjectImpl<NotificationComponent>::Stop(runtimeRemoved);
}

/**
 * Periodically sends notifications.
 *
 * @param - Event arguments for the timer.
 */
void NotificationComponent::NotificationTimerHandler()
{
	Log(LogCritical, "DEBUG", "lolwtf 1");

	double now = Utility::GetTime();

	/* Function already checks whether 'api' feature is enabled. */
	Endpoint::Ptr myEndpoint = Endpoint::GetLocalEndpoint();

	Log(LogCritical, "DEBUG", "lolwtf 2");

	for (const Notification::Ptr& notification : ConfigType::GetObjectsByType<Notification>()) {
		Log(LogCritical, "DEBUG", "lolwtf 2.1");
		if (!notification->IsActive()) {
			Log(LogCritical, "DEBUG", "lolwtf 2.1.1");
			continue;
		}

		Log(LogCritical, "DEBUG", "lolwtf 2.2");

		String notificationName = notification->GetName();
		Log(LogCritical, "DEBUG", "lolwtf 2.3");
		bool updatedObjectAuthority = ApiListener::UpdatedObjectAuthority();
		Log(LogCritical, "DEBUG", "lolwtf 2.4");

		/* Skip notification if paused, in a cluster setup & HA feature is enabled. */
		if (notification->IsPaused()) {
			Log(LogCritical, "DEBUG", "lolwtf 2.4.1");
			if (updatedObjectAuthority) {
				Log(LogCritical, "DEBUG", "lolwtf 2.4.1.1");
				auto stashedNotifications (notification->GetStashedNotifications());
				Log(LogCritical, "DEBUG", "lolwtf 2.4.1.2");
				ObjectLock olock(stashedNotifications);

				Log(LogCritical, "DEBUG", "lolwtf 2.4.1.3");
				if (stashedNotifications->GetLength()) {
					Log(LogCritical, "DEBUG", "lolwtf 2.4.1.3.1");
					Log(LogNotice, "NotificationComponent")
						<< "Notification '" << notificationName << "': HA cluster active, this endpoint does not have the authority. Dropping all stashed notifications.";

					stashedNotifications->Clear();
					Log(LogCritical, "DEBUG", "lolwtf 2.4.1.3.2");
				}
				Log(LogCritical, "DEBUG", "lolwtf 2.4.1.4");
			}

			Log(LogCritical, "DEBUG", "lolwtf 2.4.2");
			if (myEndpoint && GetEnableHA()) {
				Log(LogCritical, "DEBUG", "lolwtf 2.4.2.1");
				Log(LogNotice, "NotificationComponent")
					<< "Reminder notification '" << notificationName << "': HA cluster active, this endpoint does not have the authority (paused=true). Skipping.";
				continue;
			}
		}

		Log(LogCritical, "DEBUG", "lolwtf 2.5");

		Checkable::Ptr checkable = notification->GetCheckable();

		Log(LogCritical, "DEBUG", "lolwtf 2.6");

		if (!IcingaApplication::GetInstance()->GetEnableNotifications() || !checkable->GetEnableNotifications())
			continue;

		Log(LogCritical, "DEBUG", "lolwtf 2.7");

		bool reachable = checkable->IsReachable(DependencyNotification);

		Log(LogCritical, "DEBUG", "lolwtf 2.8");

		if (reachable) {
			Log(LogCritical, "DEBUG", "lolwtf 2.8.1");
			Array::Ptr unstashedNotifications = new Array();

			{
				Log(LogCritical, "DEBUG", "lolwtf 2.8.1.1");
				auto stashedNotifications (notification->GetStashedNotifications());
				Log(LogCritical, "DEBUG", "lolwtf 2.8.1.2");
				ObjectLock olock(stashedNotifications);

				Log(LogCritical, "DEBUG", "lolwtf 2.8.1.3");
				stashedNotifications->CopyTo(unstashedNotifications);
				Log(LogCritical, "DEBUG", "lolwtf 2.8.1.4");
				stashedNotifications->Clear();
			}

			ObjectLock olock(unstashedNotifications);

			Log(LogCritical, "DEBUG", "lolwtf 2.8.2");
			for (Dictionary::Ptr unstashedNotification : unstashedNotifications) {
				Log(LogCritical, "DEBUG", "lolwtf 2.8.2.1");
				try {
					Log(LogCritical, "DEBUG", "lolwtf 2.8.2.1.1");
					Log(LogNotice, "NotificationComponent")
						<< "Attempting to send stashed notification '" << notificationName << "'.";

					notification->BeginExecuteNotification(
						(NotificationType)(int)unstashedNotification->Get("type"),
						(CheckResult::Ptr)unstashedNotification->Get("cr"),
						(bool)unstashedNotification->Get("force"),
						(bool)unstashedNotification->Get("reminder"),
						(String)unstashedNotification->Get("author"),
						(String)unstashedNotification->Get("text")
					);

					Log(LogCritical, "DEBUG", "lolwtf 2.8.2.1.2");
				} catch (const std::exception& ex) {
					Log(LogCritical, "DEBUG", "lolwtf 2.8.2.2");
					Log(LogWarning, "NotificationComponent")
						<< "Exception occurred during notification for object '"
						<< notificationName << "': " << DiagnosticInformation(ex, false);
				}
				Log(LogCritical, "DEBUG", "lolwtf 2.8.2.3");
			}
		}

		Log(LogCritical, "DEBUG", "lolwtf 2.9");

		if (notification->GetInterval() <= 0 && notification->GetNoMoreNotifications()) {
			Log(LogCritical, "DEBUG", "lolwtf 2.9.1");
			Log(LogNotice, "NotificationComponent")
				<< "Reminder notification '" << notificationName << "': Notification was sent out once and interval=0 disables reminder notifications.";
			continue;
		}

		Log(LogCritical, "DEBUG", "lolwtf 2.10");
		if (notification->GetNextNotification() > now)
			continue;


		Log(LogCritical, "DEBUG", "lolwtf 2.11");
		{
			ObjectLock olock(notification);
			Log(LogCritical, "DEBUG", "lolwtf 2.11.1");
			notification->SetNextNotification(Utility::GetTime() + notification->GetInterval());
		}

		Log(LogCritical, "DEBUG", "lolwtf 2.12");
		{
			Host::Ptr host;
			Service::Ptr service;
			tie(host, service) = GetHostService(checkable);
			Log(LogCritical, "DEBUG", "lolwtf 2.12.1");

			ObjectLock olock(checkable);

			Log(LogCritical, "DEBUG", "lolwtf 2.12.2");
			if (checkable->GetStateType() == StateTypeSoft)
				continue;

			Log(LogCritical, "DEBUG", "lolwtf 2.12.3");
			/* Don't send reminder notifications for OK/Up states. */
			if ((service && service->GetState() == ServiceOK) || (!service && host->GetState() == HostUp))
				continue;

			Log(LogCritical, "DEBUG", "lolwtf 2.12.4");

			/* Skip in runtime filters. */
			if (!reachable || checkable->IsInDowntime() || checkable->IsAcknowledged() || checkable->IsFlapping())
				continue;

			Log(LogCritical, "DEBUG", "lolwtf 2.12.5");
		}

		Log(LogCritical, "DEBUG", "lolwtf 2.13");
		try {
			Log(LogCritical, "DEBUG", "lolwtf 2.13.1.1");
			Log(LogNotice, "NotificationComponent")
				<< "Attempting to send reminder notification '" << notificationName << "'.";

			Log(LogCritical, "DEBUG", "lolwtf 2.13.1.2");
			notification->BeginExecuteNotification(NotificationProblem, checkable->GetLastCheckResult(), false, true);
			Log(LogCritical, "DEBUG", "lolwtf 2.13.1.3");
		} catch (const std::exception& ex) {

			Log(LogCritical, "DEBUG", "lolwtf 2.13.2.1");
			Log(LogWarning, "NotificationComponent")
				<< "Exception occurred during notification for object '"
				<< notificationName << "': " << DiagnosticInformation(ex, false);
		}
		Log(LogCritical, "DEBUG", "lolwtf 2.14");
	}
	Log(LogCritical, "DEBUG", "lolwtf 3");
}

/**
 * Processes icinga::SendNotifications messages.
 */
void NotificationComponent::SendNotificationsHandler(const Checkable::Ptr& checkable, NotificationType type,
	const CheckResult::Ptr& cr, const String& author, const String& text)
{
	checkable->SendNotifications(type, cr, author, text);
}
