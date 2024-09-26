#include "detours.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"
#include "convar.h"
#include "tier0/icommandline.h"

CModule::~CModule()
{
	if ( m_pCVar )
		delete m_pCVar; // Could this cause a crash? idk.

	if ( m_pCVarName )
		delete[] m_pCVarName;

	if ( m_pDebugCVar )
		delete m_pDebugCVar; // Could this cause a crash? idk either. But it didn't. Yet. Or has it.

	if ( m_pDebugCVarName )
		delete[] m_pDebugCVarName;
}

void OnModuleConVarChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	CModule* pModule = (CModule*)g_pModuleManager.FindModuleByConVar((ConVar*)convar);
	if (!pModule)
	{
		Warning("holylib: Failed to find CModule for convar %s!\n", convar->GetName());
		return;
	}

	pModule->SetEnabled(((ConVar*)convar)->GetBool(), true);
}

void OnModuleDebugConVarChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	CModule* pModule = (CModule*)g_pModuleManager.FindModuleByConVar((ConVar*)convar);
	if (!pModule)
	{
		Warning("holylib: Failed to find CModule for convar %s!\n", convar->GetName());
		return;
	}

	pModule->GetModule()->SetDebug(((ConVar*)convar)->GetInt());
}

void CModule::SetModule(IModule* pModule)
{
	m_bStartup = true;
	m_pModule = pModule;
	m_pModule->SetWrapper(this);
#ifdef SYSTEM_LINUX // Welcome to ifdef hell
#if ARCHITECTURE_IS_X86
	if (pModule->Compatibility() & LINUX32)
		m_bCompatible = true;
#else
	if (pModule->Compatibility() & LINUX64)
		m_bCompatible = true;
#endif
#else
#if ARCHITECTURE_IS_X86
	if (pModule->Compatibility() & WINDOWS32)
		m_bCompatible = true;
#else
	if (pModule->Compatibility() & WINDOWS64)
		m_bCompatible = true;
#endif
#endif

	std::string pStrName = "holylib_enable_";
	pStrName.append(pModule->Name());
	std::string cmdStr = "-";
	cmdStr.append(pStrName);
	int cmd = CommandLine()->ParmValue(cmdStr.c_str(), -1);
	if (cmd > -1)
		SetEnabled(cmd == 1, true);
	else
		if (CommandLine()->ParmValue("-holylib_startdisabled", -1) == -1)
			m_bEnabled = m_pModule->IsEnabledByDefault() ? m_bCompatible : false;

	m_pCVarName = new char[48];
	V_strncpy(m_pCVarName, pStrName.c_str(), 48);
	m_pCVar = new ConVar(m_pCVarName, m_bEnabled ? "1" : "0", FCVAR_ARCHIVE, "Whether this module should be active or not", OnModuleConVarChange);

	std::string pDebugStrName = "holylib_debug_";
	pDebugStrName.append(pModule->Name());

	m_pDebugCVarName = new char[48];
	V_strncpy(m_pDebugCVarName, pDebugStrName.c_str(), 48);
	m_pDebugCVar = new ConVar(m_pDebugCVarName, "0", FCVAR_ARCHIVE, "Whether this module will show debug stuff", OnModuleDebugConVarChange);

	int cmdDebug = CommandLine()->ParmValue(((std::string)"-" + pDebugStrName).c_str(), -1);
	if (cmdDebug > -1)
	{
		m_pModule->SetDebug(cmdDebug);
		m_pDebugCVar->SetValue(cmdDebug);
	}

	m_bStartup = false;
}

void CModule::SetEnabled(bool bEnabled, bool bForced)
{
	if (m_bEnabled != bEnabled)
	{
		if (bEnabled && !m_bEnabled)
		{
			if (!m_bCompatible)
			{
				Warning("holylib: module %s is not compatible with this platform!\n", m_pModule->Name());

				if (!bForced)
					return;
			}

			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_Init)
				m_pModule->Init(&g_pModuleManager.GetAppFactory(), &g_pModuleManager.GetGameFactory());

			if (status & LoadStatus_DetourInit)
			{
				m_pModule->InitDetour(true); // I want every module to be able to be disabled/enabled properly
				m_pModule->InitDetour(false);
			}

			if (status & LoadStatus_LuaInit)
				m_pModule->LuaInit(false);

			if (status & LoadStatus_LuaServerInit)
				m_pModule->LuaInit(true);

			if (!m_bStartup)
				Msg("holylib: Enabled module %s\n", m_pModule->Name());
		} else {
			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_LuaInit)
				m_pModule->LuaShutdown();

			if (status & LoadStatus_Init)
				Shutdown();

			if (!m_bStartup)
				Msg("holylib: Disabled module %s\n", m_pModule->Name());
		}
	}

	m_bEnabled = bEnabled;
}

void CModule::Shutdown()
{
	Detour::Remove(m_pModule->m_pID);
	m_pModule->Shutdown();
}

CModuleManager::CModuleManager() // ToDo: Look into how IGameSystem works and use something similar. I don't like to add each one manually
{
	RegisterModule(pHolyLibModule);
	RegisterModule(pGameeventLibModule);
	RegisterModule(pServerPluginLibModule);
	RegisterModule(pSourceTVLibModule);
	RegisterModule(pThreadPoolFixModule);
	RegisterModule(pStringTableModule);
	RegisterModule(pPrecacheFixModule);
	RegisterModule(pPVSModule);
	RegisterModule(pSurfFixModule);
	RegisterModule(pFileSystemModule);
	RegisterModule(pUtilModule);
	RegisterModule(pConCommandModule);
	RegisterModule(pVProfModule);
	RegisterModule(pCVarsModule);
	RegisterModule(pBitBufModule);
	RegisterModule(pNetworkingModule);
	RegisterModule(pSteamWorksModule);
	RegisterModule(pPASModule);
	RegisterModule(pBassModule);
	RegisterModule(pSysTimerModule);
	RegisterModule(pVoiceChatModule);
	RegisterModule(pPhysEnvModule);
}

int g_pIDs = 0;
void CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	CModule* module = new CModule();
	module->SetModule(pModule);
	module->SetID(g_pIDs);
	Msg("holylib: Registered module %-*s (%-*i Enabled: %s Compatible: %s)\n", 
		15,
		module->FastGetModule()->Name(), 
		2,
		g_pIDs,
		module->FastIsEnabled() ? "true, " : "false,",
		module->IsCompatible() ? "true " : "false"
	);

	m_pModules.push_back(module);
}

IModuleWrapper* CModuleManager::FindModuleByConVar(ConVar* convar)
{
	for (CModule* pModule : m_pModules)
	{
		if (convar == pModule->GetConVar())
			return pModule;

		if (convar == pModule->GetDebugConVar())
			return pModule;
	}

	return NULL;
}

IModuleWrapper* CModuleManager::FindModuleByName(const char* name)
{
	for (CModule* pModule : m_pModules)
	{
		if (V_stricmp(pModule->FastGetModule()->Name(), name) == 0)
			return pModule;
	}

	return NULL;
}

IModuleWrapper* CModuleManager::FindModuleByModule(IModule* pTargetModule)
{
	for (CModule* pModule : m_pModules)
	{
		if (pModule->FastGetModule() == pTargetModule)
			return pModule;
	}

	return NULL;
}

void CModuleManager::Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn)
{
	m_pAppFactory = appfn;
	m_pGameFactory = gamefn;
}

void CModuleManager::Init()
{
	if (!(m_pStatus & LoadStatus_PreDetourInit))
	{
		DevMsg("holylib: ghostinj didn't call InitDetour! Calling it now\n");
		InitDetour(true);
	}

	m_pStatus |= LoadStatus_Init;
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->FastIsEnabled() ) { continue; }
		pModule->FastGetModule()->Init(&GetAppFactory(), &GetGameFactory());
	}
}

void CModuleManager::LuaInit(bool bServerInit)
{
	if (bServerInit)
		m_pStatus |= LoadStatus_LuaServerInit;
	else
		m_pStatus |= LoadStatus_LuaInit;

	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->FastIsEnabled() ) { continue; }
		pModule->FastGetModule()->LuaInit(bServerInit);
	}
}

void CModuleManager::LuaShutdown()
{
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->FastIsEnabled() ) { continue; }
		pModule->FastGetModule()->LuaShutdown();
	}
}

void CModuleManager::InitDetour(bool bPreServer)
{
	if (bPreServer)
		m_pStatus |= LoadStatus_PreDetourInit;
	else
		m_pStatus |= LoadStatus_DetourInit;

	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->FastIsEnabled() ) { continue; }
		pModule->FastGetModule()->InitDetour(bPreServer);
	}
}

void CModuleManager::Think(bool bSimulating)
{
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->FastIsEnabled() ) { continue; }
		pModule->FastGetModule()->Think(bSimulating);
	}
}

void CModuleManager::Shutdown()
{
	m_pStatus = 0;
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->FastIsEnabled() ) { continue; }
		pModule->Shutdown();
	}
}

CModuleManager g_pModuleManager;