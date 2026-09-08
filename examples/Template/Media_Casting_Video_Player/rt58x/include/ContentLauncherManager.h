/*
 *    Content Launcher cluster (0x050A) delegate for the Casting Video Player (0x0023).
 *    Minimal application implementation: accepts LaunchContent / LaunchURL and reports
 *    a small set of accepted headers / supported streaming protocols.
 */

#pragma once

#include <app/clusters/content-launch-server/content-launch-server.h>

class ContentLauncherManager : public chip::app::Clusters::ContentLauncher::Delegate
{
public:
    ContentLauncherManager() = default;

    void HandleLaunchContent(
        chip::app::CommandResponseHelper<chip::app::Clusters::ContentLauncher::Commands::LauncherResponse::Type> & helper,
        const chip::app::DataModel::DecodableList<chip::app::Clusters::ContentLauncher::Parameter> & parameterList, bool autoplay,
        const chip::CharSpan & data,
        const chip::Optional<chip::app::Clusters::ContentLauncher::PlaybackPreferences> playbackPreferences,
        bool useCurrentContext) override;

    void HandleLaunchUrl(
        chip::app::CommandResponseHelper<chip::app::Clusters::ContentLauncher::Commands::LauncherResponse::Type> & helper,
        const chip::CharSpan & contentUrl, const chip::CharSpan & displayString,
        const chip::app::Clusters::ContentLauncher::BrandingInformation & brandingInformation) override;

    CHIP_ERROR HandleGetAcceptHeaderList(chip::app::AttributeValueEncoder & aEncoder) override;
    uint32_t HandleGetSupportedStreamingProtocols() override;

    uint32_t GetFeatureMap(chip::EndpointId endpoint) override;
    uint16_t GetClusterRevision(chip::EndpointId endpoint) override;
};
