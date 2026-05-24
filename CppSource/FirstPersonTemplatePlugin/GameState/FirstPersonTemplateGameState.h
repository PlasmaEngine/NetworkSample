#pragma once

#include <Core/Input/Declarations.h>
#include <Core/World/Declarations.h>
#include <FirstPersonTemplatePlugin/FirstPersonTemplatePluginDLL.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/GameState/FallbackGameState.h>
#include <GameEngine/GameState/GameState.h>
#include <NetworkPlugin/WorldModule/NetworkWorldModule.h>

class FirstPersonTemplateGameState : public plGameState, public plNetworkObject
{
  PL_ADD_DYNAMIC_REFLECTION(FirstPersonTemplateGameState, plGameState);

public:
  FirstPersonTemplateGameState();
  ~FirstPersonTemplateGameState();

  virtual void ProcessInput() override;

  void OnNetworkStateChanged(plNetworkConnectionState::Enum state) override;
  void OnClientConnecting(plNetworkClientConnectionData& pClient) override;
  void OnClientConnected(plNetworkClient* pClient) override;
  void OnClientDisconnected(plNetworkClient* pClient) override;
protected:
  virtual void ConfigureInputActions() override;
  virtual void ConfigureMainCamera() override;
  virtual plResult SpawnPlayer(plStringView sStartPosition, const plTransform& startPositionOffset) override;
  virtual void OnChangedMainWorld(plWorld* pPrevWorld, plWorld* pNewWorld, plStringView sStartPosition, const plTransform& startPositionOffset) override;
  virtual plString GetStartupSceneFile() override;

private:
  virtual void OnActivation(plWorld* pWorld, plStringView sStartPosition, const plTransform& startPositionOffset) override;
  virtual void OnDeactivation() override;
  virtual void BeforeWorldUpdate() override;
  virtual void AfterWorldUpdate() override;

  void HostGame();
  plConsoleFunction<void()> m_HostGame;

  void JoinGame();
  plConsoleFunction<void()> m_JoinGame;

  plNetworkWorldModule* GetOrCreateNetworkModule();
  void RegisterNetworkObject(plNetworkWorldModule* pNetModule);
  void RegisterNetworkPlayerPrefab(plNetworkWorldModule* pNetModule, plStringView sStartPosition = {}, const plTransform& startPositionOffset = plTransform::MakeIdentity());
  plResult SpawnNetworkPlayer(plUInt32 uiOwnerClientID, plStringView sStartPosition, const plTransform& startPositionOffset);
  plResult GetNetworkPlayerSpawn(plStringView sStartPosition, const plTransform& startPositionOffset, plStringBuilder& out_sPrefab, plTransform& out_transform) const;

  bool m_bRegisteredNetworkObject = false;
};
