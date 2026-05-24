#include <FirstPersonTemplatePlugin/FirstPersonTemplatePluginPCH.h>

#include <Core/Input/InputManager.h>
#include <Core/System/Window.h>
#include <Core/World/World.h>
#include <FirstPersonTemplatePlugin/GameState/FirstPersonTemplateGameState.h>
#include <Foundation/Configuration/CVar.h>
#include <Foundation/Logging/Log.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <GameEngine/Gameplay/PlayerStartPointComponent.h>

plCVarBool cvar_DebugDisplay("FirstPersonTemplate.DebugDisplay", false, plCVarFlags::Default, "Whether the game should display debug geometry.");
plCVarString cvar_NetworkServerAddress("FirstPersonTemplate.Network.ServerAddress", "127.0.0.1", plCVarFlags::Default, "The address to use when hosting a network game.");
plCVarInt cvar_NetworkServerPort("FirstPersonTemplate.Network.ServerPort", 7777, plCVarFlags::Default, "The port to use when hosting a network game.");
plCVarString cvar_NetworkPlayerPrefab("FirstPersonTemplate.Network.PlayerPrefab", "", plCVarFlags::Default, "Prefab resource path or GUID to use for network-spawned players. Empty falls back to the active player start prefab.");

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(FirstPersonTemplateGameState, 1, plRTTIDefaultAllocator<FirstPersonTemplateGameState>)
PL_END_DYNAMIC_REFLECTED_TYPE;

FirstPersonTemplateGameState::FirstPersonTemplateGameState()
  : m_HostGame("HostGame", "Host a network Game", plMakeDelegate(&FirstPersonTemplateGameState::HostGame, this))
  , m_JoinGame("JoinGame", "Join a network Game, Takes IP address", plMakeDelegate(&FirstPersonTemplateGameState::JoinGame, this))
{

}

FirstPersonTemplateGameState::~FirstPersonTemplateGameState() = default;

plString FirstPersonTemplateGameState::GetStartupSceneFile()
{
  // replace this to load a certain scene at startup
  // the default implementation looks at the command line "-scene" argument

  // if we have a "-scene" command line argument, it was launched from the editor and we should load that
  if (plCommandLineUtils::GetGlobalInstance()->HasOption("-scene"))
  {
    return plCommandLineUtils::GetGlobalInstance()->GetStringOption("-scene");
  }

  // otherwise, we use the hardcoded 'Main.plScene'
  // if that doesn't exist, this function has to be adjusted
  // note that you can return an asset GUID here, instead of a path
  return "AssetCache/Common/Scenes/Main.plBinScene";
}

void FirstPersonTemplateGameState::OnActivation(plWorld* pWorld, plStringView sStartPosition, const plTransform& startPositionOffset)
{
  PL_LOG_BLOCK("GameState::Activate");

  SUPER::OnActivation(pWorld, sStartPosition, startPositionOffset);
}

void FirstPersonTemplateGameState::OnDeactivation()
{
  if (m_pMainWorld)
  {
    PL_LOCK(m_pMainWorld->GetWriteMarker());
    auto* pNetModule = m_pMainWorld->GetModule<plNetworkWorldModule>();
    if (pNetModule)
    {
      const plNetworkDisconnectReason::Enum reason = pNetModule->IsClient() ? plNetworkDisconnectReason::ClientQuit : plNetworkDisconnectReason::ServerShutdown;
      pNetModule->Stop(reason);

      if (m_bRegisteredNetworkObject)
      {
        pNetModule->RemoveNetworkObject(this);
        m_bRegisteredNetworkObject = false;
      }
    }
  }

  plGameState::OnDeactivation();
}

void FirstPersonTemplateGameState::AfterWorldUpdate()
{
  SUPER::AfterWorldUpdate();
}

void FirstPersonTemplateGameState::HostGame()
{
  PL_LOCK(m_pMainWorld->GetWriteMarker());
  auto* pNetModule = GetOrCreateNetworkModule();
  if (!pNetModule)
    return;

  plNetworkConfig config;
  config.m_sServerAddress = cvar_NetworkServerAddress;
  config.m_uiServerPort = cvar_NetworkServerPort;
  config.m_uiConnectionLimit = 4;

  pNetModule->SetNetworkConfig(config);
  pNetModule->SetGameVersion(1);
  RegisterNetworkObject(pNetModule);
  RegisterNetworkPlayerPrefab(pNetModule);

  pNetModule->StartHost();
}

void FirstPersonTemplateGameState::JoinGame()
{
  PL_LOCK(m_pMainWorld->GetWriteMarker());
  auto* pNetModule = GetOrCreateNetworkModule();
  if (!pNetModule)
    return;

  plNetworkConfig config;
  config.m_sServerAddress = cvar_NetworkServerAddress;
  config.m_uiServerPort = cvar_NetworkServerPort;

  pNetModule->SetNetworkConfig(config);
  pNetModule->SetGameVersion(1);
  RegisterNetworkObject(pNetModule);
  RegisterNetworkPlayerPrefab(pNetModule);
  pNetModule->StartClient();
}

void FirstPersonTemplateGameState::BeforeWorldUpdate()
{
  SUPER::BeforeWorldUpdate();


  // if you need to modify the world, this is a good place to do it
}

plResult FirstPersonTemplateGameState::SpawnPlayer(plStringView sStartPosition, const plTransform& startPositionOffset)
{
  if (m_pMainWorld == nullptr)
    return PL_FAILURE;

  PL_LOCK(m_pMainWorld->GetWriteMarker());

  auto* pNetModule = m_pMainWorld->GetModule<plNetworkWorldModule>();
  if (pNetModule && pNetModule->IsConnected())
  {
    if (pNetModule->IsClient())
      return PL_SUCCESS;

    return SpawnNetworkPlayer(pNetModule->GetLocalClientID(), sStartPosition, startPositionOffset);
  }

  return PL_SUCCESS;
}

void FirstPersonTemplateGameState::OnChangedMainWorld(plWorld* pPrevWorld, plWorld* pNewWorld, plStringView sStartPosition, const plTransform& startPositionOffset)
{
  SUPER::OnChangedMainWorld(pPrevWorld, pNewWorld, sStartPosition, startPositionOffset);

  // called whenever the main world is changed, ie when transitioning between levels
  // may need to update references to the world here or reset some state
}

void FirstPersonTemplateGameState::ConfigureInputActions()
{
  SUPER::ConfigureInputActions();
}

void FirstPersonTemplateGameState::ProcessInput()
{
  SUPER::ProcessInput();

  plWorld* pWorld = m_pMainWorld;

}

void FirstPersonTemplateGameState::ConfigureMainCamera()
{
  SUPER::ConfigureMainCamera();

}

void FirstPersonTemplateGameState::OnNetworkStateChanged(plNetworkConnectionState::Enum state)
{
  plNetworkObject::OnNetworkStateChanged(state);
}

void FirstPersonTemplateGameState::OnClientConnecting(plNetworkClientConnectionData& pClient)
{
  plNetworkObject::OnClientConnecting(pClient);
}

void FirstPersonTemplateGameState::OnClientConnected(plNetworkClient* pClient)
{
  plNetworkObject::OnClientConnected(pClient);

  PL_LOCK(m_pMainWorld->GetWriteMarker());

  auto* pNetModule = m_pMainWorld->GetModule<plNetworkWorldModule>();
  if (!pNetModule || !pClient || (!pNetModule->IsHost() && !pNetModule->IsServer()))
    return;

  SpawnNetworkPlayer(pClient->m_uiClientID, {}, plTransform::MakeIdentity()).IgnoreResult();
}

void FirstPersonTemplateGameState::OnClientDisconnected(plNetworkClient* pClient)
{
  plNetworkObject::OnClientDisconnected(pClient);

  PL_LOCK(m_pMainWorld->GetWriteMarker());

  auto* pNetModule = m_pMainWorld->GetModule<plNetworkWorldModule>();
  if (!pNetModule || !pClient || (!pNetModule->IsHost() && !pNetModule->IsServer()))
    return;

  pNetModule->GetObjectManager()->DespawnObjectsOwnedBy(pClient->m_uiClientID);
}

plNetworkWorldModule* FirstPersonTemplateGameState::GetOrCreateNetworkModule()
{
  if (m_pMainWorld == nullptr)
    return nullptr;

  return m_pMainWorld->GetOrCreateModule<plNetworkWorldModule>();
}

void FirstPersonTemplateGameState::RegisterNetworkObject(plNetworkWorldModule* pNetModule)
{
  if (!pNetModule || m_bRegisteredNetworkObject)
    return;

  pNetModule->RegisterNetworkObject(this);
  m_bRegisteredNetworkObject = true;
}

void FirstPersonTemplateGameState::RegisterNetworkPlayerPrefab(plNetworkWorldModule* pNetModule, plStringView sStartPosition, const plTransform& startPositionOffset)
{
  if (!pNetModule)
    return;

  plStringBuilder sPrefab;
  plTransform spawnTransform;
  if (GetNetworkPlayerSpawn(sStartPosition, startPositionOffset, sPrefab, spawnTransform).Failed())
    return;

  plHashedString sPrefabGuid;
  sPrefabGuid.Assign(sPrefab.GetData());
  pNetModule->GetObjectManager()->RegisterNetworkPrefab(sPrefabGuid);
}

plResult FirstPersonTemplateGameState::SpawnNetworkPlayer(plUInt32 uiOwnerClientID, plStringView sStartPosition, const plTransform& startPositionOffset)
{
  auto* pNetModule = m_pMainWorld ? m_pMainWorld->GetModule<plNetworkWorldModule>() : nullptr;
  if (!pNetModule || (!pNetModule->IsHost() && !pNetModule->IsServer()))
    return PL_FAILURE;

  plStringBuilder sPrefab;
  plTransform spawnTransform = plTransform::MakeIdentity();
  if (GetNetworkPlayerSpawn(sStartPosition, startPositionOffset, sPrefab, spawnTransform).Failed())
    return PL_FAILURE;

  RegisterNetworkPlayerPrefab(pNetModule, sStartPosition, startPositionOffset);

  plHashedString sPrefabGuid;
  sPrefabGuid.Assign(sPrefab.GetData());
  const plUInt32 uiNetworkID = pNetModule->GetObjectManager()->SpawnNetworkObject(sPrefabGuid, spawnTransform, uiOwnerClientID, plNetworkObjectRole::OwnedAuthoritative);
  return uiNetworkID != 0 ? PL_SUCCESS : PL_FAILURE;
}

plResult FirstPersonTemplateGameState::GetNetworkPlayerSpawn(plStringView sStartPosition, const plTransform& startPositionOffset, plStringBuilder& out_sPrefab, plTransform& out_transform) const
{
  if (m_pMainWorld == nullptr)
    return PL_FAILURE;

  out_sPrefab = cvar_NetworkPlayerPrefab.GetValue();

  plPlayerStartPointComponent* pBestComp = nullptr;
  if (auto* pMan = m_pMainWorld->GetComponentManager<plPlayerStartPointComponentManager>())
  {
    for (auto it = pMan->GetComponents(); it.IsValid(); ++it)
    {
      if (!it->IsActive())
        continue;

      if (pBestComp == nullptr)
      {
        pBestComp = it;
      }
      else if (it->GetOwner()->GetName().IsEqual_NoCase(sStartPosition))
      {
        pBestComp = it;
      }
      else if (!pBestComp->GetOwner()->GetName().IsEqual_NoCase(sStartPosition) && it->GetOwner()->GetName().IsEmpty())
      {
        pBestComp = it;
      }
    }
  }

  if (pBestComp)
  {
    out_transform = plTransform::MakeGlobalTransform(pBestComp->GetOwner()->GetGlobalTransform(), startPositionOffset);

    if (out_sPrefab.IsEmpty())
      out_sPrefab = pBestComp->GetPlayerPrefabFile();
  }
  else
  {
    out_transform = startPositionOffset;
  }

  if (sStartPosition.IsEqual_NoCase("GlobalOverride"))
    out_transform = startPositionOffset;

  out_transform.m_vScale.Set(1.0f);

  if (out_sPrefab.IsEmpty())
  {
    plLog::Error("FirstPersonTemplate: Cannot spawn network player because FirstPersonTemplate.Network.PlayerPrefab is empty and no player start prefab was found.");
    return PL_FAILURE;
  }

  return PL_SUCCESS;
}
