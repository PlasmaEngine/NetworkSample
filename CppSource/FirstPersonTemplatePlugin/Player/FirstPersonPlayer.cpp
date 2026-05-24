#include <FirstPersonTemplatePlugin/FirstPersonTemplatePluginPCH.h>

#include <FirstPersonTemplatePlugin/Player/FirstPersonPlayer.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GameEngine/Gameplay/InputComponent.h>
#include <GameEngine/Physics/CharacterControllerComponent.h>
#include <JoltPlugin/Character/JoltCharacterControllerComponent.h>
#include <GameEngine/Gameplay/BlackboardComponent.h>
#include <GameComponentsPlugin/Gameplay/HeadBoneComponent.h>

namespace
{
  constexpr plUInt8 FIRST_PERSON_PLAYER_INPUT_MESSAGE_ID = plNetworkMessageIDs::UserMessageStart;
}

// clang-format off
PL_BEGIN_COMPONENT_TYPE(FirstPersonPlayer, 1 /* version */, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("InputSendRate", m_fInputSendRate)->AddAttributes(new plDefaultValueAttribute(30.0f), new plClampValueAttribute(1.0f, 120.0f)),
  }
  PL_END_PROPERTIES;

  PL_BEGIN_MESSAGEHANDLERS
  {
    PL_MESSAGE_HANDLER(plMsgNetworkUserMessage, OnNetworkUserMessage),
    PL_MESSAGE_HANDLER(plMsgNetworkObjectSpawned, OnNetworkObjectSpawned),
  }
  PL_END_MESSAGEHANDLERS;

  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("Game/Player"), // Component menu group
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE
// clang-format on

FirstPersonPlayer::FirstPersonPlayer()
  : m_pInputComponent(nullptr)
  , m_pCharacterControllerComponent(nullptr)
  , m_pHeadboneComonent(nullptr)
  , m_pBlackboardComponent(nullptr)
{

}

FirstPersonPlayer::~FirstPersonPlayer()
{

}

void FirstPersonPlayer::OnSimulationStarted()
{
  plNetworkedTransformComponent::OnSimulationStarted();
  SetNetworkRole(plNetworkRoleEnum::Client);
  SetChannelType(plNetworkChannelTypeEnum::UnreliableOrdered);

  if (!GetOwner()->TryGetComponentOfBaseType(m_pInputComponent))
  {
    plLog::Error("ERROR: Not input component found on the player");
  }

  if (!GetOwner()->TryGetComponentOfBaseType(m_pCharacterControllerComponent))
  {
    plLog::Error("ERROR: Not character component found on the player.");
  }

  plGameObject* pCameraObject = GetOwner()->FindChildByName("Camera", true);
  if (!pCameraObject)
  {
    plLog::Error("ERROR: Camera object not found on the player.");
    return;
  }

  if (!pCameraObject->TryGetComponentOfBaseType(m_pHeadboneComonent))
  {
    plLog::Error("ERROR: Not headbone component found on the player.");
  }

  if (!GetOwner()->TryGetComponentOfBaseType(m_pBlackboardComponent))
  {
    plLog::Error("ERROR: Not blackboard component found on the player.");
  }
}

void FirstPersonPlayer::SerializeComponent(plWorldWriter& stream) const
{

}

void FirstPersonPlayer::DeserializeComponent(plWorldReader& stream)
{

}

void FirstPersonPlayer::NetworkSerialize(plNetworkMessage& msg)
{
  msg.WriteUInt32(m_uiInputSequence);
  msg.WriteFloat(m_ClientInputToSend.m_fMoveForwards);
  msg.WriteFloat(m_ClientInputToSend.m_fMoveBackwards);
  msg.WriteFloat(m_ClientInputToSend.m_fStrafeLeft);
  msg.WriteFloat(m_ClientInputToSend.m_fStrafeRight);
  msg.WriteFloat(m_ClientInputToSend.m_fRotateLeft);
  msg.WriteFloat(m_ClientInputToSend.m_fRotateRight);
  msg.WriteFloat(m_ClientInputToSend.m_fLookDelta);
  msg.WriteUInt8(m_ClientInputToSend.m_bJump ? 1 : 0);
  msg.WriteUInt8(m_ClientInputToSend.m_bCrouch ? 1 : 0);
  msg.WriteUInt8(m_ClientInputToSend.m_bRun ? 1 : 0);
}

void FirstPersonPlayer::NetworkDeserialize(plNetworkMessage& msg)
{
  if (!m_pNetworkModule || (!m_pNetworkModule->IsHost() && !m_pNetworkModule->IsServer()))
    return;

  msg.ReadUInt32(); // input sequence
  m_LastServerInput.m_fMoveForwards = msg.ReadFloat();
  m_LastServerInput.m_fMoveBackwards = msg.ReadFloat();
  m_LastServerInput.m_fStrafeLeft = msg.ReadFloat();
  m_LastServerInput.m_fStrafeRight = msg.ReadFloat();
  m_LastServerInput.m_fRotateLeft = msg.ReadFloat();
  m_LastServerInput.m_fRotateRight = msg.ReadFloat();
  m_LastServerInput.m_fLookDelta = msg.ReadFloat();
  m_LastServerInput.m_bJump = msg.ReadUInt8() != 0;
  m_LastServerInput.m_bCrouch = msg.ReadUInt8() != 0;
  m_LastServerInput.m_bRun = msg.ReadUInt8() != 0;
  m_LastServerInputTime = plTime::Now();
}

void FirstPersonPlayer::Update()
{
  RefreshOwnershipState();

  if (m_pNetworkModule == nullptr || m_pNetworkModule->GetPeer() == nullptr)
  {
    InputState input;
    ReadLocalInput(input);
    ApplyInput(input);
    return;
  }

  if (m_pNetworkModule->IsHost() || m_pNetworkModule->IsServer())
  {
    if (m_bIsLocalOwner)
    {
      ReadLocalInput(m_LastServerInput);
      ApplyInput(m_LastServerInput);
      UpdateAsAuthority();
    }
    else
    {
      // Client-owned player: render the owner's authoritative transform, do not simulate from input.
      UpdateAsRemote();
    }
    return;
  }

  if (m_pNetworkModule->IsClient())
  {
    if (m_bIsLocalOwner)
    {
      InputState input;
      ReadLocalInput(input);
      ApplyInput(input);
      SendInputToServer(input);
      UpdateAsAuthority();
      return;
    }

    UpdateAsRemote();
  }
}

void FirstPersonPlayer::OnAuthorityDetermined(bool bIsLocalAuthority)
{
  m_bIsLocalOwner = bIsLocalAuthority;

  // Remote entities: clients buffer ~2 send intervals for smooth interpolation;
  // host/server display remote owners with minimal delay so positions match the owner.
  plSnapshotInterpolationConfig config = m_Interpolator.GetConfig();
  config.m_fTeleportThreshold = m_fTeleportThreshold;

  if (!bIsLocalAuthority && m_pNetworkModule && (m_pNetworkModule->IsHost() || m_pNetworkModule->IsServer()))
  {
    config.m_InterpolationDelay = plTime::MakeFromMilliseconds(30.0);
  }
  else if (!bIsLocalAuthority)
  {
    config.m_InterpolationDelay = plTime::MakeFromMilliseconds(
      (m_fSendRate > 0.0f) ? (2000.0 / m_fSendRate) : 100.0);
  }

  m_Interpolator.SetConfig(config);
}

void FirstPersonPlayer::ApplyRemotePosition(const plVec3& vPosition)
{
  if (m_pCharacterControllerComponent)
  {
    m_pCharacterControllerComponent->TeleportToPosition(vPosition);
  }

  GetOwner()->SetGlobalPosition(vPosition);
}

void FirstPersonPlayer::ApplyRemoteRotation(const plQuat& qRotation)
{
  GetOwner()->SetGlobalRotation(qRotation);
}

void FirstPersonPlayer::ReadLocalInput(InputState& out_input) const
{
  if (m_pInputComponent == nullptr)
    return;

  out_input.m_bJump = m_pInputComponent->GetCurrentInputState("Jump", true) > 0.5;
  out_input.m_fMoveForwards = m_pInputComponent->GetCurrentInputState("MoveForwards", false);
  out_input.m_fMoveBackwards = m_pInputComponent->GetCurrentInputState("MoveBackwards", false);
  out_input.m_fStrafeLeft = m_pInputComponent->GetCurrentInputState("StrafeLeft", false);
  out_input.m_fStrafeRight = m_pInputComponent->GetCurrentInputState("StrafeRight", false);
  out_input.m_fRotateLeft = m_pInputComponent->GetCurrentInputState("RotateLeft", false);
  out_input.m_fRotateRight = m_pInputComponent->GetCurrentInputState("RotateRight", false);
  out_input.m_bCrouch = m_pInputComponent->GetCurrentInputState("Crouch", false) > 0.5;
  out_input.m_bRun = m_pInputComponent->GetCurrentInputState("Run", false) > 0.5;

  const float headUp = m_pInputComponent->GetCurrentInputState("LookUp", false);
  const float headDown = m_pInputComponent->GetCurrentInputState("LookDown", false);
  out_input.m_fLookDelta = headDown - headUp;
}

void FirstPersonPlayer::ApplyInput(const InputState& input)
{
  if (m_pCharacterControllerComponent)
  {
    plMsgMoveCharacterController msg;

    msg.m_bJump = input.m_bJump;
    msg.m_fMoveForwards = input.m_fMoveForwards;
    msg.m_fMoveBackwards = input.m_fMoveBackwards;
    msg.m_fStrafeLeft = input.m_fStrafeLeft;
    msg.m_fStrafeRight = input.m_fStrafeRight;
    msg.m_fRotateLeft = input.m_fRotateLeft;
    msg.m_fRotateRight = input.m_fRotateRight;
    msg.m_bCrouch = input.m_bCrouch;
    msg.m_bRun = input.m_bRun;

    m_pCharacterControllerComponent->SendMessage(msg);
  }

  if (m_pHeadboneComonent)
  {
    m_pHeadboneComonent->ChangeVerticalRotation(input.m_fLookDelta);
  }
}

void FirstPersonPlayer::SendInputToServer(const InputState& input)
{
  if (!m_pNetworkModule || !m_pNetworkModule->GetPeer() || GetNetID() == 0)
    return;

  const plTime now = plTime::Now();
  const double sendIntervalMs = (m_fInputSendRate > 0.0f) ? (1000.0 / m_fInputSendRate) : 33.333;
  if ((now - m_LastInputSendTime).GetMilliseconds() < sendIntervalMs)
    return;
  m_LastInputSendTime = now;

  m_ClientInputToSend = input;
  ++m_uiInputSequence;
  MarkDirty();
}

void FirstPersonPlayer::OnNetworkUserMessage(plMsgNetworkUserMessage& msg)
{
  if (msg.m_uiMessageID != FIRST_PERSON_PLAYER_INPUT_MESSAGE_ID || msg.m_pNetworkMessage == nullptr || !m_pNetworkModule)
    return;

  if (!m_pNetworkModule->IsHost() && !m_pNetworkModule->IsServer())
    return;

  plNetworkMessage& netMsg = *msg.m_pNetworkMessage;
  const plUInt32 uiOriginalPosition = netMsg.m_uiPosition;
  const plUInt32 uiTargetNetID = netMsg.ReadUInt32();
  if (uiTargetNetID != GetNetID())
  {
    netMsg.m_uiPosition = uiOriginalPosition;
    return;
  }

  if (!ValidateInputSender(msg))
  {
    netMsg.m_uiPosition = uiOriginalPosition;
    return;
  }

  netMsg.ReadUInt32(); // input sequence
  m_LastServerInput.m_fMoveForwards = netMsg.ReadFloat();
  m_LastServerInput.m_fMoveBackwards = netMsg.ReadFloat();
  m_LastServerInput.m_fStrafeLeft = netMsg.ReadFloat();
  m_LastServerInput.m_fStrafeRight = netMsg.ReadFloat();
  m_LastServerInput.m_fRotateLeft = netMsg.ReadFloat();
  m_LastServerInput.m_fRotateRight = netMsg.ReadFloat();
  m_LastServerInput.m_fLookDelta = netMsg.ReadFloat();
  m_LastServerInput.m_bJump = netMsg.ReadUInt8() != 0;
  m_LastServerInput.m_bCrouch = netMsg.ReadUInt8() != 0;
  m_LastServerInput.m_bRun = netMsg.ReadUInt8() != 0;
  m_LastServerInputTime = plTime::Now();
  netMsg.m_uiPosition = uiOriginalPosition;
}

void FirstPersonPlayer::OnNetworkObjectSpawned(plMsgNetworkObjectSpawned& msg)
{
  if (msg.m_uiNetworkID == GetNetID())
  {
    RefreshOwnershipState();
  }
}

void FirstPersonPlayer::RefreshOwnershipState()
{
  bool bIsLocalOwner = false;
  if (!TryDetermineLocalOwner(bIsLocalOwner))
    return;

  if (!m_bOwnershipInitialized || m_bIsLocalOwner != bIsLocalOwner)
  {
    m_bIsLocalOwner = bIsLocalOwner;
    m_bOwnershipInitialized = true;
    OnAuthorityDetermined(m_bIsLocalOwner);
  }
}

bool FirstPersonPlayer::TryDetermineLocalOwner(bool& out_bIsLocalOwner) const
{
  if (!m_pNetworkModule)
  {
    out_bIsLocalOwner = true;
    return true;
  }

  if (GetNetID() == 0)
    return false;

  const plNetworkObjectInfo* pInfo = m_pNetworkModule->GetObjectManager()->GetObjectInfo(GetNetID());
  if (pInfo)
  {
    out_bIsLocalOwner = pInfo->m_uiOwnerClientID == m_pNetworkModule->GetLocalClientID();
    return true;
  }

  if (m_pNetworkModule->IsHost() || m_pNetworkModule->IsServer())
  {
    out_bIsLocalOwner = true;
    return true;
  }

  return false;
}

bool FirstPersonPlayer::ValidateInputSender(const plMsgNetworkUserMessage& msg) const
{
  if (!m_pNetworkModule)
    return false;

  plNetworkClient* pClient = m_pNetworkModule->GetClient(msg.m_Sender);
  if (pClient == nullptr)
    return false;

  const plNetworkObjectInfo* pInfo = m_pNetworkModule->GetObjectManager()->GetObjectInfo(GetNetID());
  if (pInfo == nullptr || pInfo->m_uiOwnerClientID != pClient->m_uiClientID)
  {
    plLog::Warning("FirstPersonTemplate: rejected input from client {} for player NetID {}", pClient->m_uiClientID, GetNetID());
    return false;
  }

  return true;
}
