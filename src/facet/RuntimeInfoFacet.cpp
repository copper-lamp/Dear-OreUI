#include "facet/RuntimeInfoFacet.h"

#include "api/manifest/JsonManifestParser.h"

#include <map>

namespace dearoreui::facet {

RuntimeInfoFacet::RuntimeInfoFacet(api::IRuntimeApi const& runtime) : mRuntime(runtime) {}

api::Result<std::string> RuntimeInfoFacet::handle(std::string_view /*args*/) {
    auto info = mRuntime.getInfo();

    std::map<std::string, api::JsonValue> object;
    object.emplace("protocolVersion", api::JsonValue{static_cast<double>(info.protocolVersion)});
    object.emplace("modVersion", api::JsonValue{info.modVersion.toString()});
    object.emplace("runtimeState", api::JsonValue{info.runtimeState});
    object.emplace("minecraftVersion", api::JsonValue{info.minecraftVersion});
    object.emplace("oreuiVersion", api::JsonValue{info.oreuiVersion});
    object.emplace("coherentVersion", api::JsonValue{info.coherentVersion});
    object.emplace("ready", api::JsonValue{info.ready});

    return api::serializeJson(api::JsonValue{std::move(object)});
}

} // namespace dearoreui::facet
