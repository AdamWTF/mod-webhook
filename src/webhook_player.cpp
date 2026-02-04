#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "WebhookMgr.h"


// Add player scripts
class WebhookPlayerScripts : public PlayerScript
{
public:
    WebhookPlayerScripts() : PlayerScript("WebhookPlayerScripts") { }

    void OnPlayerUpdate(Player* player, uint32 p_time) override
    {
       // 1. Get player data
        uint32 guid = player->GetGUID().GetCounter();
        std::string name = player->GetName();
        float x = player->GetPositionX();
        float y = player->GetPositionY();

        // 2. Build the JSON object string
        std::stringstream json;
        json << "{"
             << "\"guid\": " << guid << ", "
             << "\"name\": \"" << name << "\", "
             << "\"position_x\": " << x << ", "
             << "\"position_y\": " << y
             << "}";

       sWebhookMgr->ScheduleMessage(json.str()); // Schedule the message
    }
};

// Add all scripts in one
void AddWebhookPlayerScripts()
{
    new WebhookPlayerScripts();
}