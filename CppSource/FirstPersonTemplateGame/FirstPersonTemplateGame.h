#pragma once

#include <GameEngine/GameApplication/GameApplication.h>

class FirstPersonTemplateGame : public plGameApplication
{
public:
  using SUPER = plGameApplication;

  FirstPersonTemplateGame();

protected:
  virtual void Run_InputUpdate() override;
  virtual plResult BeforeCoreSystemsStartup() override;
  virtual void AfterCoreSystemsStartup() override;
  virtual plUniquePtr<plGameStateBase> CreateGameState() override;

private:
  plResult TryProjectFolder(plStringView sPath);
  void DetermineProjectPath();
};