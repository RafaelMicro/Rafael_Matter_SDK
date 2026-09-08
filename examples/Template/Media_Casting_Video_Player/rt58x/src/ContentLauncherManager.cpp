/*
 *    Content Launcher cluster (0x050A) delegate implementation.
 */

#include "ContentLauncherManager.h"

#include <app-common/zap-generated/cluster-objects.h>
#include <lib/support/CodeUtils.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::ContentLauncher;

// Accepted content-type headers advertised by this player.
constexpr const char * kAcceptHeaders[] = { "video/mp4", "application/x-mpegURL" };

// Supported streaming protocols bitmap: DASH (bit 0) + HLS (bit 1).
constexpr uint32_t kSupportedStreamingProtocols = 0x03;

// Feature map: ContentSearch (bit 0) + URLPlayback (bit 1).
constexpr uint32_t kFeatureMap      = 0x03;
constexpr uint16_t kClusterRevision = 2;

void ContentLauncherManager::HandleLaunchContent(CommandResponseHelper<Commands::LauncherResponse::Type> & helper,
                                                 const DataModel::DecodableList<Parameter> & parameterList, bool autoplay,
                                                 const CharSpan & data, const Optional<PlaybackPreferences> playbackPreferences,
                                                 bool useCurrentContext)
{
    ChipLogProgress(Zcl, "ContentLauncherManager: LaunchContent (autoplay=%d)", autoplay);
    Commands::LauncherResponse::Type response;
    response.status = StatusEnum::kSuccess;
    helper.Success(response);
}

void ContentLauncherManager::HandleLaunchUrl(CommandResponseHelper<Commands::LauncherResponse::Type> & helper,
                                             const CharSpan & contentUrl, const CharSpan & displayString,
                                             const BrandingInformation & brandingInformation)
{
    ChipLogProgress(Zcl, "ContentLauncherManager: LaunchURL '%.*s'", static_cast<int>(contentUrl.size()), contentUrl.data());
    Commands::LauncherResponse::Type response;
    response.status = StatusEnum::kSuccess;
    helper.Success(response);
}

CHIP_ERROR ContentLauncherManager::HandleGetAcceptHeaderList(AttributeValueEncoder & aEncoder)
{
    return aEncoder.EncodeList([](const auto & encoder) -> CHIP_ERROR {
        for (const char * header : kAcceptHeaders)
        {
            ReturnErrorOnFailure(encoder.Encode(CharSpan::fromCharString(header)));
        }
        return CHIP_NO_ERROR;
    });
}

uint32_t ContentLauncherManager::HandleGetSupportedStreamingProtocols()
{
    return kSupportedStreamingProtocols;
}

uint32_t ContentLauncherManager::GetFeatureMap(chip::EndpointId endpoint)
{
    return kFeatureMap;
}

uint16_t ContentLauncherManager::GetClusterRevision(chip::EndpointId endpoint)
{
    return kClusterRevision;
}
