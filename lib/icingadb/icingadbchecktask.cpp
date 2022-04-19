/* Icinga 2 | (c) 2022 Icinga GmbH | GPLv2+ */

#include "icingadb/icingadbchecktask.hpp"
#include "icinga/host.hpp"
#include "icinga/checkcommand.hpp"
#include "icinga/macroprocessor.hpp"
#include "remote/apilistener.hpp"
#include "remote/endpoint.hpp"
#include "remote/zone.hpp"
#include "base/function.hpp"
#include "base/utility.hpp"
#include "base/perfdatavalue.hpp"
#include "base/configtype.hpp"
#include "base/convert.hpp"
#include <utility>

using namespace icinga;

REGISTER_FUNCTION_NONCONST(Internal, IcingadbCheck, &IcingadbCheckTask::ScriptFunc, "checkable:cr:resolvedMacros:useResolvedMacros");

static void ReportIcingadbCheck(
	const Checkable::Ptr& checkable, const CheckCommand::Ptr& commandObj,
	const CheckResult::Ptr& cr, String output, ServiceState state = ServiceUnknown)
{
	if (Checkable::ExecuteCommandProcessFinishedHandler) {
		double now = Utility::GetTime();
		ProcessResult pr;
		pr.PID = -1;
		pr.Output = std::move(output);
		pr.ExecutionStart = now;
		pr.ExecutionEnd = now;
		pr.ExitStatus = state;

		Checkable::ExecuteCommandProcessFinishedHandler(commandObj->GetName(), pr);
	} else {
		cr->SetState(state);
		cr->SetOutput(output);
		checkable->ProcessCheckResult(cr);
	}
}

void IcingadbCheckTask::ScriptFunc(const Checkable::Ptr& checkable, const CheckResult::Ptr& cr,
	const Dictionary::Ptr& resolvedMacros, bool useResolvedMacros)
{
	ServiceState state = ServiceOK;
	CheckCommand::Ptr commandObj = CheckCommand::ExecuteOverride ? CheckCommand::ExecuteOverride : checkable->GetCheckCommand();
	Value raw_command = commandObj->GetCommandLine();

	Host::Ptr host;
	Service::Ptr service;
	tie(host, service) = GetHostService(checkable);

	MacroProcessor::ResolverList resolvers;

	if (MacroResolver::OverrideMacros)
		resolvers.emplace_back("override", MacroResolver::OverrideMacros);

	if (service)
		resolvers.emplace_back("service", service);
	resolvers.emplace_back("host", host);
	resolvers.emplace_back("command", commandObj);
	resolvers.emplace_back("icinga", IcingaApplication::GetInstance());

	String icingadbName = MacroProcessor::ResolveMacros("$icingadb_name$", resolvers, checkable->GetLastCheckResult(),
		nullptr, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	String missingDownForWarning;
	String missingDownForCritical;
	String missingIdleForWarning;
	String missingIdleForCritical;
	String missingQueriesWarning;
	String missingQueriesCritical;
	String missingPendingQueriesWarning;
	String missingPendingQueriesCritical;

	Value downForWarning = MacroProcessor::ResolveMacros("$icingadb_downfor_warning$", resolvers, checkable->GetLastCheckResult(),
	    &missingDownForWarning, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value downForCritical = MacroProcessor::ResolveMacros("$icingadb_downfor_critical$", resolvers, checkable->GetLastCheckResult(),
	    &missingDownForCritical, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value idleForWarning = MacroProcessor::ResolveMacros("$icingadb_idlefor_warning$", resolvers, checkable->GetLastCheckResult(),
	    &missingIdleForWarning, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value idleForCritical = MacroProcessor::ResolveMacros("$icingadb_idlefor_critical$", resolvers, checkable->GetLastCheckResult(),
	    &missingIdleForCritical, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value queriesWarning = MacroProcessor::ResolveMacros("$icingadb_queries_warning$", resolvers, checkable->GetLastCheckResult(),
	    &missingQueriesWarning, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value queriesCritical = MacroProcessor::ResolveMacros("$icingadb_queries_critical$", resolvers, checkable->GetLastCheckResult(),
	    &missingQueriesCritical, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value pendingQueriesWarning = MacroProcessor::ResolveMacros("$icingadb_pending_queries_warning$", resolvers, checkable->GetLastCheckResult(),
	    &missingPendingQueriesWarning, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	Value pendingQueriesCritical = MacroProcessor::ResolveMacros("$icingadb_pending_queries_critical$", resolvers, checkable->GetLastCheckResult(),
	    &missingPendingQueriesCritical, MacroProcessor::EscapeCallback(), resolvedMacros, useResolvedMacros);

	if (resolvedMacros && !useResolvedMacros)
		return;

	if (icingadbName.IsEmpty()) {
		ReportIcingadbCheck(checkable, commandObj, cr, "Attribute 'icingadb_name' must be set.");
		return;
	}

	auto conn (IcingaDB::GetByName(icingadbName));

	if (!conn) {
		ReportIcingadbCheck(checkable, commandObj, cr, "Icinga DB connection '" + icingadbName + "' does not exist.");
		return;
	}

	auto redis (conn->GetConnection());

	if (!redis->GetConnected()) {
		ReportIcingadbCheck(checkable, commandObj, cr, "Could not connect to Redis.", ServiceCritical);
		return;
	}

	Array::Ptr xRead;

	try {
		xRead = redis->GetResultOfQuery(
			{"XREAD", "STREAMS", "icingadb:heartbeat", "0-0"},
			RedisConnection::QueryPriority::Heartbeat
		);
	} catch (const std::exception& ex) {
		ReportIcingadbCheck(checkable, commandObj, cr, String("XREAD: ") + ex.what(), ServiceCritical);
		return;
	}

	if (!xRead) {
		ReportIcingadbCheck(
			checkable, commandObj, cr,
			"The Icinga DB daemon seems to have never run. (Missing heartbeat)",
			ServiceCritical
		);

		return;
	}

	Array::Ptr xReadStream = Array::Ptr(Array::Ptr(xRead->Get(0))->Get(1))->Get(0);
	auto heartbeatTime (Convert::ToLong(String(xReadStream->Get(0)).Split("-")[0]) / 1000.0);
	std::map<String, String> heartbeatData;

	IcingaDB::KvsToMap(Array::Ptr(xReadStream->Get(1)), heartbeatData);

	auto isResponsible (Convert::ToLong(heartbeatData.at("is-responsible")));
	auto responsibleSince (Convert::ToLong(heartbeatData.at("responsible-since")) / 1000.0);
	auto now (Utility::GetTime());
	auto downFor (now - heartbeatTime);
	auto idleFor ((isResponsible ? -1 : 1) * (now - responsibleSince));

	std::ostringstream msgbuf;
	double qps = redis->GetQueryCount(60) / 60.0;
	double pendingQueries = redis->GetPendingQueryCount();

	msgbuf << "Connected to Redis."
	    << " Queries per second: " << std::fixed << std::setprecision(3) << qps
	    << " Pending queries: " << std::fixed << std::setprecision(3) << pendingQueries
	    << " Icinga DB daemon last seen: " << std::fixed << std::setprecision(3) << downFor << " seconds ago"
	    << " Icinga DB daemon" << (isResponsible ? "" : " not") << " responsible for: "
	    << std::fixed << std::setprecision(3) << (now - responsibleSince) << " seconds";

	/* Check whether the thresholds have been defined and match. */

	if (missingDownForCritical.IsEmpty() && downFor > (double)downForCritical) {
		msgbuf << " Icinga DB daemon is down for " << downFor << " seconds greater than critical threshold ("
		    << downForCritical << " seconds).";

		state = ServiceCritical;
	} else if (missingDownForWarning.IsEmpty() && downFor > (double)downForWarning) {
		msgbuf << " Icinga DB daemon is down for " << downFor << " seconds greater than warning threshold ("
		    << downForWarning << " queries).";

		state = ServiceWarning;
	}

	if (missingIdleForCritical.IsEmpty() && idleFor > (double)idleForCritical) {
		msgbuf << " Icinga DB daemon is not responsible for " << idleFor << " seconds greater than critical threshold ("
		    << idleForCritical << " seconds).";

		state = ServiceCritical;
	} else if (missingIdleForWarning.IsEmpty() && idleFor > (double)idleForWarning) {
		msgbuf << " Icinga DB daemon is not responsible for " << idleFor << " seconds greater than warning threshold ("
		    << idleForWarning << " queries).";

		if (state == ServiceOK) {
			state = ServiceWarning;
		}
	}

	if (missingQueriesCritical.IsEmpty() && qps < (double)queriesCritical) {
		msgbuf << " " << qps << " queries/s lower than critical threshold (" << queriesCritical << " queries/s).";

		state = ServiceCritical;
	} else if (missingQueriesWarning.IsEmpty() && qps < (double)queriesWarning) {
		msgbuf << " " << qps << " queries/s lower than warning threshold (" << queriesWarning << " queries/s).";

		if (state == ServiceOK) {
			state = ServiceWarning;
		}
	}

	if (missingPendingQueriesCritical.IsEmpty() && pendingQueries > (double)pendingQueriesCritical) {
		msgbuf << " " << pendingQueries << " pending queries greater than critical threshold ("
		    << pendingQueriesCritical << " queries).";

		state = ServiceCritical;
	} else if (missingPendingQueriesWarning.IsEmpty() && pendingQueries > (double)pendingQueriesWarning) {
		msgbuf << " " << pendingQueries << " pending queries greater than warning threshold ("
		    << pendingQueriesWarning << " queries).";

		if (state == ServiceOK) {
			state = ServiceWarning;
		}
	}

	cr->SetPerformanceData(new Array({
		{ new PerfdataValue("queries", qps, false, "", Empty, Empty, 0) },
		{ new PerfdataValue("queries_1min", redis->GetQueryCount(60), Empty, Empty, 0) },
		{ new PerfdataValue("queries_5mins", redis->GetQueryCount(5 * 60), Empty, Empty, 0) },
		{ new PerfdataValue("queries_15mins", redis->GetQueryCount(15 * 60), Empty, Empty, 0) },
		{ new PerfdataValue("pending_queries", pendingQueries, false, "", pendingQueriesWarning, pendingQueriesCritical, 0) },
		{ new PerfdataValue("down_for", downFor, false, "seconds", downForWarning, downForCritical, 0) },
		{ new PerfdataValue("idle_for", idleFor, false, "seconds", idleForWarning, idleForCritical) }
	}));

	ReportIcingadbCheck(checkable, commandObj, cr, msgbuf.str(), state);
}
