#include <FirstPersonTemplateGame/FirstPersonTemplateGame.h>

#include <Core/Input/InputManager.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/Logging/Log.h>

// this injects the C++ main() function
PL_APPLICATION_ENTRY_POINT(FirstPersonTemplateGame);

FirstPersonTemplateGame::FirstPersonTemplateGame()
  : plGameApplication("FirstPersonTemplate", nullptr)
{
}

plResult FirstPersonTemplateGame::TryProjectFolder(plStringView sPath)
{
  plStringBuilder sProjDir = sPath;
  sProjDir.MakeCleanPath();

  plStringBuilder sProjFile;
  sProjFile.SetPath(sProjDir, "plProject");

  if (sProjFile.IsAbsolutePath() && plOSFile::ExistsFile(sProjFile))
  {
    m_sAppProjectPath = sProjDir;
    return PL_SUCCESS;
  }

  return PL_FAILURE;
}

void FirstPersonTemplateGame::DetermineProjectPath()
{
  // IMPORTANT!
  //
  // The project path has to be set for the plGameApplication to know where the main 'project' data directory is.
  // Without it, nothing will work (the game plugin won't be loaded etc).
  //
  // The path can be relative to the '>SDK' directory (the root folder where PL is located).
  // It may also be absolute (though this isn't portable across machines).
  // Or it can be relative to plOSFile::GetApplicationDirectory() (where the Game.exe is).
  //
  // If your project is inside the PL directory, use a relative path from there.
  // If it is somewhere outside, you either need to use an absolute path or some other way to locate it.
  //
  // Note that in a final exported build the project folder is always merged with the PL data folders into one package.

  // this path works for exported projects, because during export the project folder is always copied there
  plStringBuilder sProjDir;
  if (plFileSystem::ResolveSpecialDirectory(">sdk/Data/project", sProjDir).Succeeded())
  {
    if (TryProjectFolder(sProjDir).Succeeded())
      return;
  }

#ifdef GAME_PROJECT_FOLDER
  // this absolute path will only work on the machine where the game is compiled,
  // but it works for projects that are located outside the plEngine folder
  if (TryProjectFolder(PL_PP_STRINGIFY(GAME_PROJECT_FOLDER)).Succeeded())
    return;
#endif

  // in other cases, try this relative path
  m_sAppProjectPath = "Data/Samples/FirstPersonTemplate";
}

plUniquePtr<plGameStateBase> FirstPersonTemplateGame::CreateGameState()
{
  // usually we should only have a single non-fallback gamestate which is automatically picked
  // but if necessary, we can override this here
  return SUPER::CreateGameState();
}

plResult FirstPersonTemplateGame::BeforeCoreSystemsStartup()
{
  plStartup::AddApplicationTag("game");

  PL_SUCCEED_OR_RETURN(SUPER::BeforeCoreSystemsStartup());

  DetermineProjectPath();

  return PL_SUCCESS;
}

void FirstPersonTemplateGame::AfterCoreSystemsStartup()
{
  ExecuteInitFunctions();

  plStartup::StartupHighLevelSystems();

  ActivateGameState(nullptr, {}, plTransform::MakeIdentity());
}

void FirstPersonTemplateGame::Run_InputUpdate()
{
  SUPER::Run_InputUpdate();

  if (auto pGameState = GetActiveGameState())
  {
    // pass through the closing of the application
    if (pGameState->WasQuitRequested())
    {
      RequestQuit();
    }
  }
}