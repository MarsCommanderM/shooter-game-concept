#include <STWGameplay/AudioFeedbackPresentation.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/std/containers/array.h>
#include <MiniAudio/MiniAudioBus.h>
#include <MiniAudio/MiniAudioConstants.h>
#include <MiniAudio/MiniAudioPlaybackBus.h>
#include <MiniAudio/SoundAsset.h>

#include <cmath>

namespace STWGameplay
{
    namespace
    {
        constexpr size_t ChannelCount = static_cast<size_t>(AudioFeedbackEventType::Count);
        constexpr AZ::u32 SampleRate = 22050;
        constexpr float SoundDurationSeconds = 0.075f;
        constexpr size_t MaxRetainedEvents = 128;

        class RuntimeSoundAsset final
            : public MiniAudio::SoundAsset
        {
        public:
            explicit RuntimeSoundAsset(const AZ::Data::AssetId& assetId)
                : MiniAudio::SoundAsset()
            {
                m_assetId = assetId;
                m_status = AssetStatus::Ready;
            }
        };

        void WriteU16(AZStd::vector<AZ::u8>& data, size_t offset, AZ::u16 value)
        {
            data[offset] = static_cast<AZ::u8>(value & 0xffu);
            data[offset + 1] = static_cast<AZ::u8>((value >> 8u) & 0xffu);
        }

        void WriteU32(AZStd::vector<AZ::u8>& data, size_t offset, AZ::u32 value)
        {
            for (size_t byte = 0; byte < 4; ++byte)
            {
                data[offset + byte] = static_cast<AZ::u8>((value >> (8u * byte)) & 0xffu);
            }
        }

        void WriteFourCC(AZStd::vector<AZ::u8>& data, size_t offset, const char (&value)[5])
        {
            for (size_t byte = 0; byte < 4; ++byte)
            {
                data[offset + byte] = static_cast<AZ::u8>(value[byte]);
            }
        }

        AZStd::vector<AZ::u8> BuildSyntheticWav(float frequency, float amplitude)
        {
            const size_t sampleCount = static_cast<size_t>(SampleRate * SoundDurationSeconds);
            const size_t payloadBytes = sampleCount * sizeof(AZ::s16);
            AZStd::vector<AZ::u8> data(44 + payloadBytes, 0);
            WriteFourCC(data, 0, "RIFF");
            WriteU32(data, 4, static_cast<AZ::u32>(36 + payloadBytes));
            WriteFourCC(data, 8, "WAVE");
            WriteFourCC(data, 12, "fmt ");
            WriteU32(data, 16, 16);
            WriteU16(data, 20, 1);
            WriteU16(data, 22, 1);
            WriteU32(data, 24, SampleRate);
            WriteU32(data, 28, SampleRate * sizeof(AZ::s16));
            WriteU16(data, 32, sizeof(AZ::s16));
            WriteU16(data, 34, 16);
            WriteFourCC(data, 36, "data");
            WriteU32(data, 40, static_cast<AZ::u32>(payloadBytes));

            for (size_t sample = 0; sample < sampleCount; ++sample)
            {
                const float time = static_cast<float>(sample) / static_cast<float>(SampleRate);
                const float envelope = 1.0f - static_cast<float>(sample) / static_cast<float>(sampleCount);
                const float value = std::sin(6.28318530718f * frequency * time) * amplitude * envelope;
                const auto pcm = static_cast<AZ::s16>(value * 32767.0f);
                WriteU16(data, 44 + sample * sizeof(AZ::s16), static_cast<AZ::u16>(pcm));
            }

            return data;
        }

        AZ::Data::Asset<MiniAudio::SoundAsset> CreateSyntheticSound(const char* name, float frequency, float amplitude)
        {
            const AZ::Data::AssetId assetId(AZ::Uuid::CreateName(name), MiniAudio::SoundAsset::AssetSubId);
            auto* soundAsset = aznew RuntimeSoundAsset(assetId);
            soundAsset->m_data = BuildSyntheticWav(frequency, amplitude);
            return AZ::Data::Asset<MiniAudio::SoundAsset>(soundAsset, AZ::Data::AssetLoadBehavior::NoLoad);
        }

        size_t EventIndex(AudioFeedbackEventType eventType)
        {
            return static_cast<size_t>(eventType);
        }

        float EventVolume(AudioFeedbackEventType eventType)
        {
            switch (eventType)
            {
            case AudioFeedbackEventType::Fire:
                return 0.22f;
            case AudioFeedbackEventType::Reload:
                return 0.16f;
            case AudioFeedbackEventType::Hit:
                return 0.18f;
            case AudioFeedbackEventType::Impact:
                return 0.14f;
            case AudioFeedbackEventType::EnemyState:
                return 0.13f;
            case AudioFeedbackEventType::Movement:
                return 0.08f;
            default:
                return 0.1f;
            }
        }
    }

    struct AudioFeedbackPresentation::Impl
    {
        struct Channel
        {
            AZStd::unique_ptr<AZ::Entity> m_entity;
            AZ::Data::Asset<MiniAudio::SoundAsset> m_asset;
        };

        AZStd::array<Channel, ChannelCount> m_channels;
    };

    AudioFeedbackPresentation::AudioFeedbackPresentation()
        : m_impl(AZStd::make_unique<Impl>())
    {
        m_eventSequence.reserve(MaxRetainedEvents);
    }

    AudioFeedbackPresentation::~AudioFeedbackPresentation()
    {
        Deactivate();
    }

    bool AudioFeedbackPresentation::Activate()
    {
        m_presentationActive = true;
        TryInitializeBackend();
        return true;
    }

    void AudioFeedbackPresentation::Deactivate()
    {
        for (auto& channel : m_impl->m_channels)
        {
            if (channel.m_entity)
            {
                if (channel.m_entity->GetState() == AZ::Entity::State::Active)
                {
                    channel.m_entity->Deactivate();
                }
                channel.m_entity.reset();
            }
            channel.m_asset.Reset();
        }
        m_backendReady = false;
        m_presentationActive = false;
    }

    void AudioFeedbackPresentation::TryInitializeBackend()
    {
        if (m_backendReady || MiniAudio::MiniAudioInterface::Get() == nullptr
            || MiniAudio::MiniAudioInterface::Get()->GetSoundEngine() == nullptr)
        {
            return;
        }

        constexpr float frequencies[ChannelCount] = { 880.0f, 330.0f, 1320.0f, 220.0f, 440.0f, 176.0f };
        constexpr float amplitudes[ChannelCount] = { 0.45f, 0.38f, 0.34f, 0.32f, 0.28f, 0.22f };
        constexpr const char* names[ChannelCount] = {
            "STW.Audio.Synthetic.Fire",
            "STW.Audio.Synthetic.Reload",
            "STW.Audio.Synthetic.Hit",
            "STW.Audio.Synthetic.Impact",
            "STW.Audio.Synthetic.EnemyState",
            "STW.Audio.Synthetic.Movement"
        };

        bool initialized = true;
        for (size_t index = 0; index < ChannelCount; ++index)
        {
            auto& channel = m_impl->m_channels[index];
            if (!channel.m_entity)
            {
                channel.m_entity = AZStd::make_unique<AZ::Entity>("STWAudioPresentationChannel");
                if (channel.m_entity->CreateComponent(AZ::Uuid::CreateString(MiniAudio::MiniAudioPlaybackComponentTypeId)) == nullptr)
                {
                    channel.m_entity.reset();
                    initialized = false;
                    continue;
                }
                channel.m_entity->Init();
                channel.m_entity->Activate();
                channel.m_asset = CreateSyntheticSound(names[index], frequencies[index], amplitudes[index]);
                MiniAudio::MiniAudioPlaybackRequestBus::Event(
                    channel.m_entity->GetId(),
                    &MiniAudio::MiniAudioPlaybackRequestBus::Events::SetSoundAsset,
                    channel.m_asset);
                MiniAudio::MiniAudioPlaybackRequestBus::Event(
                    channel.m_entity->GetId(),
                    &MiniAudio::MiniAudioPlaybackRequestBus::Events::SetVolumePercentage,
                    100.0f);
            }
        }

        m_backendReady = initialized;
    }

    bool AudioFeedbackPresentation::Update(float deltaTime, const AudioFeedbackInput& input)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f || !input.m_impactPosition.IsFinite())
        {
            return false;
        }

        if (!m_presentationActive)
        {
            Activate();
        }
        TryInitializeBackend();

        if (input.m_reset)
        {
            Reset();
        }

        m_movementTimer = AZStd::max(0.0f, m_movementTimer - deltaTime);
        if (input.m_shotFired && !m_previousShotFired)
        {
            Emit(AudioFeedbackEventType::Fire, EventVolume(AudioFeedbackEventType::Fire));
        }
        if (input.m_reloading && !m_previousReloading)
        {
            Emit(AudioFeedbackEventType::Reload, EventVolume(AudioFeedbackEventType::Reload));
        }
        if (input.m_hitConfirmed && !m_previousHitConfirmed)
        {
            Emit(AudioFeedbackEventType::Hit, EventVolume(AudioFeedbackEventType::Hit));
        }
        if (input.m_impactEvent && !m_previousImpactEvent)
        {
            Emit(AudioFeedbackEventType::Impact, EventVolume(AudioFeedbackEventType::Impact));
        }

        const bool enemyEvent = input.m_enemyStateChanged || input.m_enemyAttackEvent || input.m_enemyDeathEvent;
        if (enemyEvent && !m_previousEnemyEvent)
        {
            Emit(AudioFeedbackEventType::EnemyState, EventVolume(AudioFeedbackEventType::EnemyState));
        }

        if (input.m_movementActive && m_movementTimer <= 0.0f && !input.m_crouched)
        {
            Emit(AudioFeedbackEventType::Movement, EventVolume(AudioFeedbackEventType::Movement));
            m_movementTimer = input.m_sprinting || input.m_sliding ? 0.24f : 0.36f;
        }

        m_previousShotFired = input.m_shotFired;
        m_previousReloading = input.m_reloading;
        m_previousHitConfirmed = input.m_hitConfirmed;
        m_previousImpactEvent = input.m_impactEvent;
        m_previousEnemyEvent = enemyEvent;
        return true;
    }

    void AudioFeedbackPresentation::Reset()
    {
        m_previousShotFired = false;
        m_previousReloading = false;
        m_previousHitConfirmed = false;
        m_previousImpactEvent = false;
        m_previousEnemyEvent = false;
        m_movementTimer = 0.0f;
        m_eventSequence.clear();
        ++m_resetCount;
    }

    void AudioFeedbackPresentation::Emit(AudioFeedbackEventType eventType, float volume)
    {
        const size_t index = EventIndex(eventType);
        ++m_eventCounts[index];
        if (m_eventSequence.size() < MaxRetainedEvents)
        {
            m_eventSequence.push_back(eventType);
        }

        if (m_backendReady)
        {
            auto& channel = m_impl->m_channels[index];
            if (channel.m_entity)
            {
                MiniAudio::MiniAudioPlaybackRequestBus::Event(
                    channel.m_entity->GetId(), &MiniAudio::MiniAudioPlaybackRequestBus::Events::SetVolumePercentage, volume * 100.0f);
                MiniAudio::MiniAudioPlaybackRequestBus::Event(
                    channel.m_entity->GetId(), &MiniAudio::MiniAudioPlaybackRequestBus::Events::Stop);
                MiniAudio::MiniAudioPlaybackRequestBus::Event(
                    channel.m_entity->GetId(), &MiniAudio::MiniAudioPlaybackRequestBus::Events::Play);
            }
        }
    }

    AZ::u32 AudioFeedbackPresentation::GetEventCount(AudioFeedbackEventType eventType) const
    {
        const size_t index = EventIndex(eventType);
        return index < m_eventCounts.size() ? m_eventCounts[index] : 0;
    }
}
