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

    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            });

        return value;
    }

    bool StartsWith(std::string const& value, std::string const& prefix)
    {
        return value.size() >= prefix.size()
            && value.compare(0, prefix.size(), prefix) == 0;
    }

    bool IsPlayerBot(Player* player)
    {
        if (!player || !player->GetSession())
        {
            return true;
        }

        uint32 accountId = player->GetSession()->GetAccountId();

        std::string accountName;
        if (!AccountMgr::GetName(accountId, accountName))
        {
            return false;
        }

        std::string botAccountPrefix = sConfigMgr->GetOption<std::string>(
            "AiPlayerbot.RandomBotAccountPrefix",
            "rndbot"
        );

        return StartsWith(ToLower(accountName), ToLower(botAccountPrefix));
    }

    void OnPlayerLogin(Player* player) override
    {
        if (IsPlayerBot(player))
        {
            return;
        }

        std::stringstream ss;
        ss << "Player " << player->GetName() << " just signed in.";

        std::string message = ss.str();

        sWebhookMgr->ScheduleMessage(message);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (IsPlayerBot(player))
        {
            return;
        }

        std::stringstream ss;
        ss << "Player " << player->GetName() << " just signed out.";

        std::string message = ss.str();

        sWebhookMgr->ScheduleMessage(message);
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (IsPlayerBot(player))
        {
            return;
        }

        std::stringstream ss;
        ss << "Player " << player->GetName() << " just reached level " << player->getLevel() << ".";

        std::string message = ss.str();

        sWebhookMgr->ScheduleMessage(message);
    }

    void OnPlayerAchievementComplete(Player* player, AchievementEntry const* achievement) override
    {
        if (IsPlayerBot(player))
        {
            return;
        }

        std::stringstream ss;
        ss << "Player " << player->GetName() << " just earned the achievement " << achievement->Title << ".";

        std::string message = ss.str();

        sWebhookMgr->ScheduleMessage(message);
    }
};

// Add all scripts in one
void AddWebhookPlayerScripts()
{
    new WebhookPlayerScripts();
}