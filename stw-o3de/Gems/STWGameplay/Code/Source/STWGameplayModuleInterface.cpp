#include "STWGameplayModuleInterface.h"

#include <AzCore/Memory/Memory.h>
#include <STWGameplay/STWGameplayTypeIds.h>
#include <Clients/STWGameplaySystemComponent.h>

namespace STWGameplay
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(STWGameplayModuleInterface, "STWGameplayModuleInterface", STWGameplayModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(STWGameplayModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(STWGameplayModuleInterface, AZ::SystemAllocator);

    STWGameplayModuleInterface::STWGameplayModuleInterface()
    {
        m_descriptors.insert(m_descriptors.end(), { STWGameplaySystemComponent::CreateDescriptor() });
    }

    AZ::ComponentTypeList STWGameplayModuleInterface::GetRequiredSystemComponents() const
    {
        return { azrtti_typeid<STWGameplaySystemComponent>() };
    }
}
