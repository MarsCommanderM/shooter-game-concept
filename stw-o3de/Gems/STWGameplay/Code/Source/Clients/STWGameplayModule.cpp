#include <STWGameplay/STWGameplayTypeIds.h>
#include <STWGameplayModuleInterface.h>

namespace STWGameplay
{
    class STWGameplayModule final : public STWGameplayModuleInterface
    {
    public:
        AZ_RTTI(STWGameplayModule, STWGameplayModuleTypeId, STWGameplayModuleInterface);
        AZ_CLASS_ALLOCATOR(STWGameplayModule, AZ::SystemAllocator);
    };
}

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), STWGameplay::STWGameplayModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_STWGameplay, STWGameplay::STWGameplayModule)
#endif
