#include <PCH/pch.h>
#include "SerializationResource.h"

#include "ECS/ECS.h"
#include "ECS/Entities/Entity.h"
#include "ECS/Components/Components.h"

#include "rfl.hpp"
#include "Utils/SerializationOperators.h"

namespace Dog
{

    template <typename... Types, typename Func>
    void for_each_type(entt::type_list<Types...>, Func&& func) {
        (func.template operator()<Types> (), ...);
    }

    void SerializationResource::Serialize(const std::string& filepath)
    {
        // Get all entities
        entt::registry& registry = ecs->GetRegistry();
        auto view = registry.view<entt::entity>();

        nlohmann::json entitiesJson;

        for (auto& entityHandle : view)
        {
            nlohmann::json entityJson;

            for_each_type(types, [&]<typename ComponentType>() 
            {
                if (auto* component = registry.try_get<ComponentType>(entityHandle))
                {
                    auto cName = entt::type_name<ComponentType>::value();
                    if (const auto pos = cName.rfind("::"); pos != std::string_view::npos)
                    {
                        cName = cName.substr(pos + 2);
                    }

                    nlohmann::json componentJson;
                    auto component_view = rfl::to_view(*component);
                    component_view.apply([&](const auto& f) 
                    {
                        componentJson[f.name()] = *f.value();
                    });

                    entityJson[cName].push_back(componentJson);
                }
            });

            entitiesJson["Entities"].push_back(entityJson);
        }

        std::ofstream file(filepath);
        if (file.is_open())
        {
            file << entitiesJson.dump();
            file.close();
            DOG_INFO("Scene serialized to {0}", filepath);
        }
        else
        {
            DOG_ERROR("Failed to open file {0} for writing", filepath);
        }
    }

    void SerializationResource::Deserialize(const std::string& filepath)
    {
        currentScenePath = filepath;

        ecs->GetRegistry().clear();

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            DOG_ERROR("Failed to open scene file for deserialization: {0}", filepath);
            return;
        }

        nlohmann::json j;
        try
        {
            file >> j;
        }
        catch (nlohmann::json::parse_error& e)
        {
            DOG_ERROR("Failed to parse scene file {0}: {1}", filepath, e.what());
            return;
        }

        if (!j.contains("Entities"))
        {
            DOG_WARN("No 'Entities' found in scene file: {0}", filepath);
            return;
        }

        // Create a map to hold deserialization functions, keyed by component name.
        std::unordered_map<std::string, std::function<void(Entity, const nlohmann::json&)>> deserializers;

        // Create a generic lambda to register a component deserializer.
        auto register_deserializer = [&deserializers]<typename ComponentType>() 
        {
            auto cName = entt::type_name<ComponentType>::value();
            if (const auto pos = cName.rfind("::"); pos != std::string_view::npos) {
                cName = cName.substr(pos + 2);
            }
            std::string componentName(cName);

            // This is the generic function that will be stored in the map.
            deserializers[componentName] = [](Entity entity, const nlohmann::json& j_component)
            {
                auto& component = entity.TryAddComponent<ComponentType>();

                auto component_view = rfl::to_view(component);
                component_view.apply([&](auto&& field) 
                {
                    if (j_component.contains(field.name())) 
                    {
                        // Get the type of the field.
                        using FieldType = std::remove_cvref_t<decltype(*field.value())>;
                        // Read from JSON, convert to the field's type, and assign.
                        *field.value() = j_component.at(field.name()).get<FieldType>();
                    }
                });
            };
        };

        // Register all serializable components automatically.
        for_each_type(types, register_deserializer);

        for (const nlohmann::json& entityData : j["Entities"])
        {
            // First, extract the entity's name from the TagComponent for creation.
            std::string entityTag = "Unnamed Entity";
            if (entityData.contains("TagComponent"))
            {
                const auto& tagComponentArray = entityData.at("TagComponent");
                if (tagComponentArray.is_array() && !tagComponentArray.empty())
                {
                    const nlohmann::json& tagObject = tagComponentArray[0];
                    if (tagObject.contains("Tag"))
                    {
                        entityTag = tagObject.at("Tag").get<std::string>();
                    }
                }
            }

            Entity entity = ecs->AddEntity(entityTag);

            // Iterate over components in the JSON and call the correct deserializer
            for (auto& [componentName, componentDataArray] : entityData.items())
            {
                if (deserializers.count(componentName))
                {
                    if (!componentDataArray.is_array() || componentDataArray.empty())
                    {
                        DOG_WARN("Component '{0}' for entity '{1}' has invalid data.", componentName, entityTag);
                        continue;
                    }
                    const nlohmann::json& componentObject = componentDataArray[0];

                    // Look up and execute the deserializer.
                    deserializers.at(componentName)(entity, componentObject);
                }
                else
                {
                    DOG_WARN("Unknown component type during deserialization: {0}", componentName);
                }
            }
        }

        DOG_INFO("Scene deserialized successfully from {0}", filepath);
    }
}
