#pragma once

#include <Core/World/Component.h>
#include <Core/World/ComponentManager.h>
#include <Core/World/World.h>
#include <NetworkPlugin/Components/NetworkedTransformComponent.h>
#include <NetworkPlugin/Core/NetworkMessages.h>

class plInputComponent;
class plJoltCharacterControllerComponent;
class plHeadBoneComponent;
class plBlackboardComponent;

using FirstPersonPlayerManager = plComponentManagerSimple<class FirstPersonPlayer, plComponentUpdateType::WhenSimulating>;

class FirstPersonPlayer : public plNetworkedTransformComponent
{
    PL_DECLARE_COMPONENT_TYPE(FirstPersonPlayer, plNetworkedTransformComponent, FirstPersonPlayerManager);

public:
    FirstPersonPlayer();
    ~FirstPersonPlayer();

    virtual void SerializeComponent(plWorldWriter& stream) const override;
    virtual void DeserializeComponent(plWorldReader& stream) override;
    virtual void NetworkSerialize(plNetworkMessage& msg) override;
    virtual void NetworkDeserialize(plNetworkMessage& msg) override;

protected:
    virtual void OnSimulationStarted() override;
    virtual void Update() override;
    virtual void OnAuthorityDetermined(bool bIsLocalAuthority) override;
    virtual void ApplyRemotePosition(const plVec3& vPosition) override;
    virtual void ApplyRemoteRotation(const plQuat& qRotation) override;

private:
    struct InputState
    {
      float m_fMoveForwards = 0.0f;
      float m_fMoveBackwards = 0.0f;
      float m_fStrafeLeft = 0.0f;
      float m_fStrafeRight = 0.0f;
      float m_fRotateLeft = 0.0f;
      float m_fRotateRight = 0.0f;
      float m_fLookDelta = 0.0f;
      bool m_bJump = false;
      bool m_bCrouch = false;
      bool m_bRun = false;
    };

    void ReadLocalInput(InputState& out_input) const;
    void ApplyInput(const InputState& input);
    void SendInputToServer(const InputState& input);
    void OnNetworkUserMessage(plMsgNetworkUserMessage& msg);
    void OnNetworkObjectSpawned(plMsgNetworkObjectSpawned& msg);
    void RefreshOwnershipState();
    bool TryDetermineLocalOwner(bool& out_bIsLocalOwner) const;
    bool ValidateInputSender(const plMsgNetworkUserMessage& msg) const;

    plInputComponent* m_pInputComponent;
    plJoltCharacterControllerComponent* m_pCharacterControllerComponent;
    plHeadBoneComponent* m_pHeadboneComonent;
    plBlackboardComponent* m_pBlackboardComponent;

    InputState m_LastServerInput;
    InputState m_ClientInputToSend;
    plTime m_LastInputSendTime;
    plTime m_LastServerInputTime;
    plUInt32 m_uiInputSequence = 0;
    float m_fInputSendRate = 30.0f;
    bool m_bIsLocalOwner = false;
    bool m_bOwnershipInitialized = false;
};
