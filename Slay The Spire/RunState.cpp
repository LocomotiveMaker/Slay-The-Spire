// -----------------------------------------------------------------------------
// @file       RunState.cpp
// -----------------------------------------------------------------------------
#include "RunState.h"
#include "ScreenManager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

namespace {

constexpr size_t kMaxPotionCount = 3;

CardData MakeCard(
    int id,
    const std::string& name,
    int cost,
    const std::string& description,
    CardType type,
    CardTargetType targetType,
    CardEffectType effectType,
    CardDiscardEffectType discardEffectType,
    int primaryValue,
    int secondaryValue) {
    CardData card = {};
    card.id = id;
    card.name = name;
    card.cost = cost;
    card.description = description;
    card.type = type;
    card.targetType = targetType;
    card.effectType = effectType;
    card.discardEffectType = discardEffectType;
    card.primaryValue = primaryValue;
    card.secondaryValue = secondaryValue;
    card.upgradeLevel = 0;
    return card;
}

RelicData MakeRelic(int id, const std::string& name, const std::string& description) {
    RelicData relic = {};
    relic.id = id;
    relic.name = name;
    relic.description = description;
    return relic;
}

PotionData MakePotion(int id, const std::string& name, const std::string& description, bool battleOnly) {
    PotionData potion = {};
    potion.id = id;
    potion.name = name;
    potion.description = description;
    potion.battleOnly = battleOnly;
    return potion;
}

std::vector<CardData> BuildGeneralCardPool() {
    return {
        MakeCard(2000, u8"강타", 1, u8"적에게 6 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 6, 0),
        MakeCard(2001, u8"강타+", 1, u8"적에게 8 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 8, 0),
        MakeCard(2002, u8"수비", 1, u8"방어도 5를 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 5, 0),
        MakeCard(2003, u8"수비+", 1, u8"방어도 7을 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 7, 0),
        MakeCard(2004, u8"재정비", 0, u8"버릴 때 카드 1장을 뽑고 에너지 1을 얻습니다.", CardType::Skill, CardTargetType::None, CardEffectType::None, CardDiscardEffectType::DrawCardsGainEnergy, 1, 1),
        MakeCard(2005, u8"악점 노출", 2, u8"적에게 취약 2를 부여합니다.", CardType::Skill, CardTargetType::Enemy, CardEffectType::ApplyVulnerable, CardDiscardEffectType::None, 2, 0),
        MakeCard(2006, u8"분쇄 타격", 2, u8"적에게 12 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 12, 0),
        MakeCard(2007, u8"굳건한 자세", 2, u8"방어도 10을 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 10, 0),
        MakeCard(2008, u8"휘몰아치기", 1, u8"적에게 7 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 7, 0)
    };
}

std::vector<RelicData> BuildRelicPool() {
    return {
        MakeRelic(3000, u8"검은 혈석", u8"공격 카드의 존재감을 강화하는 임시 유물입니다."),
        MakeRelic(3001, u8"방패 톱니", u8"방어 카드 위주의 런을 보강하는 임시 유물입니다."),
        MakeRelic(3002, u8"연금 주머니", u8"포션 획득과 활용을 돕는 임시 유물입니다."),
        MakeRelic(3003, u8"황금 이빨", u8"골드 수급과 상점 운영을 위한 임시 유물입니다."),
        MakeRelic(3004, u8"금 간 부적", u8"미지 이벤트와 보상 노드의 밀도를 올리는 임시 유물입니다.")
    };
}

std::vector<PotionData> BuildPotionPool() {
    return {
        MakePotion(4000, u8"회복 포션", u8"체력을 소량 회복합니다.", false),
        MakePotion(4001, u8"에너지 포션", u8"즉시 에너지를 회복합니다.", false),
        MakePotion(4002, u8"도주 포션", u8"보스전이 아닌 전투에서 도주합니다.", true)
    };
}

std::uint32_t BuildRoomSeed(const RunStateData& run, int salt) {
    const std::uint32_t nodeKey = static_cast<std::uint32_t>(run.currentNodeId >= 0
        ? (run.currentNodeId + 1)
        : (7000 + static_cast<int>(run.currentRoomType) * 131));

    return run.seed ^ (nodeKey * 2654435761u) ^ static_cast<std::uint32_t>(salt * 2246822519u);
}

std::mt19937 MakeRoomRng(const RunStateData& run, int salt) {
    return std::mt19937(BuildRoomSeed(run, salt));
}

template <typename T>
T PickFromPool(const std::vector<T>& pool, std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(pool.size()) - 1);
    return pool[static_cast<size_t>(dist(rng))];
}

template <typename T>
std::vector<T> PickDistinctFromPool(const std::vector<T>& pool, std::mt19937& rng, int count) {
    std::vector<T> picks;
    std::vector<int> indices;
    indices.reserve(pool.size());
    for (int index = 0; index < static_cast<int>(pool.size()); ++index) {
        indices.push_back(index);
    }

    std::shuffle(indices.begin(), indices.end(), rng);
    count = (std::min)(count, static_cast<int>(indices.size()));
    for (int index = 0; index < count; ++index) {
        picks.push_back(pool[static_cast<size_t>(indices[static_cast<size_t>(index)])]);
    }
    return picks;
}

void InitializeShopRoom(RunStateData& run) {
    if (run.shopRoom.initialized) {
        return;
    }

    std::mt19937 rng = MakeRoomRng(run, 11);
    const std::vector<CardData> cardPool = BuildGeneralCardPool();
    const std::vector<RelicData> relicPool = BuildRelicPool();
    const std::vector<PotionData> potionPool = BuildPotionPool();

    run.shopRoom = {};
    run.shopRoom.initialized = true;
    run.shopRoom.removalPrice = 75;
    run.shopRoom.noticeText = u8"카드 제거, 카드 구매, 유물 구매, 포션 구매를 한 번에 시험할 수 있습니다.";

    const std::vector<CardData> cardOffers = PickDistinctFromPool(cardPool, rng, 3);
    for (size_t index = 0; index < cardOffers.size(); ++index) {
        ShopOfferState offer = {};
        offer.id = 5000 + static_cast<int>(index);
        offer.type = ShopOfferType::Card;
        offer.card = cardOffers[index];
        offer.title = cardOffers[index].name;
        offer.description = cardOffers[index].description;
        offer.price = 45 + (cardOffers[index].cost * 15);
        run.shopRoom.offers.push_back(offer);
    }

    ShopOfferState relicOffer = {};
    relicOffer.id = 5100;
    relicOffer.type = ShopOfferType::Relic;
    relicOffer.relic = PickFromPool(relicPool, rng);
    relicOffer.title = relicOffer.relic.name;
    relicOffer.description = relicOffer.relic.description;
    relicOffer.price = 140;
    run.shopRoom.offers.push_back(relicOffer);

    ShopOfferState potionOffer = {};
    potionOffer.id = 5200;
    potionOffer.type = ShopOfferType::Potion;
    potionOffer.potion = PickFromPool(potionPool, rng);
    potionOffer.title = potionOffer.potion.name;
    potionOffer.description = potionOffer.potion.description;
    potionOffer.price = 55;
    run.shopRoom.offers.push_back(potionOffer);
}

EntityData BuildEnemyTemplateForRoom(RunNodeType type) {
    switch (type) {
    case RunNodeType::Elite:
        return { 9100, u8"수호 슬라임", 72, 72, 0, 1, 0, 0, 0 };
    case RunNodeType::Boss:
        return { 9200, u8"수호자 프로토타입", 130, 130, 0, 2, 0, 0, 0 };
    case RunNodeType::Battle:
    default:
        return { 9000, u8"훈련용 슬라임", 48, 48, 0, 0, 0, 0, 0 };
    }
}

void InitializeBattleRoom(RunStateData& run) {
    if (run.battleRoom.initialized) {
        return;
    }

    run.battleRoom = {};
    run.battleRoom.initialized = true;
    run.battleRoom.enemy = BuildEnemyTemplateForRoom(run.currentRoomType);

    switch (run.currentRoomType) {
    case RunNodeType::Elite:
        run.battleRoom.introText = u8"일반 적보다 더 빠르고 단단한 엘리트 전투입니다.";
        break;
    case RunNodeType::Boss:
        run.battleRoom.introText = u8"막의 끝을 지키는 보스전입니다.";
        break;
    case RunNodeType::Battle:
    default:
        run.battleRoom.introText = u8"실시간 카드 전투 프로토타입 전투입니다.";
        break;
    }
}

void InitializeRestRoom(RunStateData& run) {
    if (run.restRoom.initialized) {
        return;
    }

    run.restRoom = {};
    run.restRoom.initialized = true;
    run.restRoom.noticeText = u8"휴식은 즉시 구현되어 있고, 강화는 다음 단계에서 실제 효과를 붙일 예정입니다.";
}

void InitializeTreasureRoom(RunStateData& run) {
    if (run.treasureRoom.initialized) {
        return;
    }

    std::mt19937 rng = MakeRoomRng(run, 23);
    const std::vector<RelicData> relicPool = BuildRelicPool();
    const std::vector<PotionData> potionPool = BuildPotionPool();

    run.treasureRoom = {};
    run.treasureRoom.initialized = true;
    run.treasureRoom.introText = u8"보물방은 리워드처럼 보상을 고른 뒤 결과를 확인하고 돌아갑니다.";

    const std::vector<RelicData> relicChoices = PickDistinctFromPool(relicPool, rng, 2);
    for (size_t index = 0; index < relicChoices.size(); ++index) {
        TreasureChoiceState choice = {};
        choice.id = 6000 + static_cast<int>(index);
        choice.title = relicChoices[index].name;
        choice.description = relicChoices[index].description;
        choice.grantRelic = true;
        choice.relic = relicChoices[index];
        run.treasureRoom.choices.push_back(choice);
    }

    TreasureChoiceState goldChoice = {};
    goldChoice.id = 6010;
    goldChoice.title = u8"금화 더미";
    goldChoice.description = u8"즉시 골드 90을 얻습니다.";
    goldChoice.goldReward = 90;
    run.treasureRoom.choices.push_back(goldChoice);

    TreasureChoiceState potionChoice = {};
    potionChoice.id = 6011;
    potionChoice.title = u8"밀봉된 병";
    potionChoice.description = u8"포션 1개를 얻습니다. 칸이 가득 차면 실패합니다.";
    potionChoice.grantPotion = true;
    potionChoice.potion = PickFromPool(potionPool, rng);
    run.treasureRoom.choices.push_back(potionChoice);
}

EventChoiceState MakeEventChoice(
    int id,
    const std::string& label,
    const std::string& previewText,
    const std::string& resultTitle,
    const std::string& resultText,
    int hpDelta,
    int goldDelta) {
    EventChoiceState choice = {};
    choice.id = id;
    choice.label = label;
    choice.previewText = previewText;
    choice.resultTitle = resultTitle;
    choice.resultText = resultText;
    choice.hpDelta = hpDelta;
    choice.goldDelta = goldDelta;
    return choice;
}

void InitializeEventRoom(RunStateData& run) {
    if (run.eventRoom.initialized) {
        return;
    }

    std::mt19937 rng = MakeRoomRng(run, 37);
    const std::vector<RelicData> relicPool = BuildRelicPool();
    const std::vector<PotionData> potionPool = BuildPotionPool();
    const std::vector<CardData> cardPool = BuildGeneralCardPool();

    run.eventRoom = {};
    run.eventRoom.initialized = true;

    std::uniform_int_distribution<int> templateDist(0, 2);
    switch (templateDist(rng)) {
    case 0: {
        run.eventRoom.title = u8"핏빛 제단";
        run.eventRoom.description = u8"붉은 연기가 새어 나오는 제단이 길을 막고 있습니다. 무언가를 내어놓으면 강한 보상을 얻을 수 있을 것 같습니다.";
        run.eventRoom.artLines = {
            u8"      /\\      ",
            u8"     /##\\     ",
            u8"    /####\\    ",
            u8"    |####|    ",
            u8"    |_||_|    "
        };

        EventChoiceState relicChoice = MakeEventChoice(
            7000,
            u8"체력 10을 잃고 유물 획득",
            u8"강한 대가를 치르고 임시 유물 1개를 얻습니다.",
            u8"제단의 응답",
            u8"차가운 금속 조각이 손에 들어왔습니다.",
            -10,
            0);
        relicChoice.grantRelic = true;
        relicChoice.relic = PickFromPool(relicPool, rng);
        run.eventRoom.choices.push_back(relicChoice);

        run.eventRoom.choices.push_back(MakeEventChoice(
            7001,
            u8"체력 6을 잃고 골드 75 획득",
            u8"즉시 골드를 얻지만 체력이 깎입니다.",
            u8"피의 대가",
            u8"바닥에 흩어진 금화를 주워 담았습니다.",
            -6,
            75));

        run.eventRoom.choices.push_back(MakeEventChoice(
            7002,
            u8"그냥 떠난다",
            u8"아무 일도 일어나지 않습니다.",
            u8"아무 일도 없었다",
            u8"불길한 기분만 남긴 채 자리를 벗어났습니다.",
            0,
            0));
        break;
    }
    case 1: {
        run.eventRoom.title = u8"부서진 마차";
        run.eventRoom.description = u8"짐칸이 열린 채 넘어진 마차가 보입니다. 주변을 뒤지면 쓸 만한 것을 건질 수 있을지도 모릅니다.";
        run.eventRoom.artLines = {
            u8"   ________    ",
            u8" _/|_||_\\`.__  ",
            u8"(   _    _ _\\ ",
            u8"=`-(_)--(_)-'  "
        };

        EventChoiceState potionChoice = MakeEventChoice(
            7100,
            u8"골드 25를 내고 병 하나를 챙긴다",
            u8"포션 1개를 얻습니다. 골드가 부족하거나 포션 칸이 가득 차면 실패합니다.",
            u8"깨지지 않은 병",
            u8"쓸 만한 포션을 하나 손에 넣었습니다.",
            0,
            -25);
        potionChoice.grantPotion = true;
        potionChoice.potion = PickFromPool(potionPool, rng);
        run.eventRoom.choices.push_back(potionChoice);

        EventChoiceState cardChoice = MakeEventChoice(
            7101,
            u8"짐칸을 뒤져 카드 1장 획득",
            u8"무작위 카드 1장을 덱에 추가합니다.",
            u8"구겨진 설계도",
            u8"쓸 만한 카드 한 장을 챙겼습니다.",
            0,
            0);
        cardChoice.grantCard = true;
        cardChoice.card = PickFromPool(cardPool, rng);
        run.eventRoom.choices.push_back(cardChoice);

        run.eventRoom.choices.push_back(MakeEventChoice(
            7102,
            u8"건드리지 않고 지나간다",
            u8"아무 효과도 없습니다.",
            u8"빈손",
            u8"쓸데없는 위험은 피하는 편이 낫습니다.",
            0,
            0));
        break;
    }
    default: {
        run.eventRoom.title = u8"조용한 분수";
        run.eventRoom.description = u8"이상할 정도로 고요한 분수가 보입니다. 물은 맑지만 주변의 기운은 심상치 않습니다.";
        run.eventRoom.artLines = {
            u8"      __       ",
            u8"   .-'  '-.    ",
            u8"  (  물줄기 )   ",
            u8"   '-.__.-'    ",
            u8"      ||       "
        };

        run.eventRoom.choices.push_back(MakeEventChoice(
            7200,
            u8"물을 마신다",
            u8"체력을 12 회복합니다.",
            u8"맑은 숨",
            u8"차분한 기운이 몸에 스며듭니다.",
            12,
            0));

        run.eventRoom.choices.push_back(MakeEventChoice(
            7201,
            u8"분수를 뒤져 골드 55 획득",
            u8"골드를 얻지만 체력 5를 잃습니다.",
            u8"차가운 동전",
            u8"물속에 잠겨 있던 금화를 건져냈습니다.",
            -5,
            55));

        run.eventRoom.choices.push_back(MakeEventChoice(
            7202,
            u8"기도만 올리고 떠난다",
            u8"아무 효과도 없습니다.",
            u8"잔잔한 여운",
            u8"아무것도 얻지 못했지만 나쁜 일도 없었습니다.",
            0,
            0));
        break;
    }
    }
}

RunNodeType PickNodeTypeForFloor(int floor, int regularFloorCount, std::mt19937& rng) {
    if (floor == regularFloorCount) {
        std::uniform_int_distribution<int> lateFloorDist(0, 2);
        switch (lateFloorDist(rng)) {
        case 0: return RunNodeType::Battle;
        case 1: return RunNodeType::Elite;
        default: return RunNodeType::Rest;
        }
    }

    std::uniform_int_distribution<int> dist(0, 99);
    const int roll = dist(rng);

    if (floor == 1) {
        if (roll < 55) return RunNodeType::Battle;
        if (roll < 75) return RunNodeType::Event;
        return RunNodeType::Treasure;
    }

    if (roll < 35) return RunNodeType::Battle;
    if (roll < 50) return RunNodeType::Event;
    if (roll < 62) return RunNodeType::Shop;
    if (roll < 74) return RunNodeType::Rest;
    if (roll < 86) return RunNodeType::Treasure;
    return RunNodeType::Elite;
}

void GenerateRunMap(RunStateData& run, int screenWidth, int screenHeight) {
    std::mt19937 rng(run.seed);

    const int regularFloorCount = 5 + static_cast<int>(rng() % 4);
    run.totalFloors = regularFloorCount + 1;
    run.currentFloor = 0;
    run.currentNodeId = -1;
    run.nodes.clear();
    run.visitedNodeTypes.clear();

    std::vector<std::vector<int>> floorNodeIds;
    floorNodeIds.resize(static_cast<size_t>(run.totalFloors + 1));

    int nextId = 0;
    const int bottomY = screenHeight - 14;
    const int topY = 10;
    const int usableHeight = (bottomY - topY);
    const int floorSpacing = (run.totalFloors > 1) ? (usableHeight / run.totalFloors) : usableHeight;

    for (int floor = 1; floor <= regularFloorCount; ++floor) {
        const int nodeCount = (floor == 1 || floor == regularFloorCount) ? 2 : 3;
        const int leftBound = 18;
        const int rightBound = screenWidth - 18;
        const int laneWidth = (rightBound - leftBound) / (nodeCount + 1);
        const int baseY = bottomY - ((floor - 1) * floorSpacing);

        for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
            std::uniform_int_distribution<int> jitterX(-5, 5);
            std::uniform_int_distribution<int> jitterY(-2, 2);

            RunNodeState node = {};
            node.id = nextId++;
            node.floor = floor;
            node.x = leftBound + ((nodeIndex + 1) * laneWidth) + jitterX(rng);
            node.y = baseY + jitterY(rng);
            node.type = PickNodeTypeForFloor(floor, regularFloorCount, rng);
            node.unlocked = (floor == 1);

            floorNodeIds[static_cast<size_t>(floor)].push_back(node.id);
            run.nodes.push_back(node);
        }
    }

    RunNodeState bossNode = {};
    bossNode.id = nextId++;
    bossNode.floor = run.totalFloors;
    bossNode.x = screenWidth / 2;
    bossNode.y = topY;
    bossNode.type = RunNodeType::Boss;
    floorNodeIds[static_cast<size_t>(run.totalFloors)].push_back(bossNode.id);
    run.nodes.push_back(bossNode);

    for (int floor = 1; floor < run.totalFloors; ++floor) {
        const std::vector<int>& currentFloorNodes = floorNodeIds[static_cast<size_t>(floor)];
        const std::vector<int>& nextFloorNodes = floorNodeIds[static_cast<size_t>(floor + 1)];

        for (int currentNodeId : currentFloorNodes) {
            RunNodeState* currentNode = FindNodeById(run, currentNodeId);
            if (currentNode == nullptr) {
                continue;
            }

            std::vector<std::pair<int, int>> distancePairs;
            for (int nextNodeId : nextFloorNodes) {
                const RunNodeState* nextNode = FindNodeById(run, nextNodeId);
                if (nextNode == nullptr) {
                    continue;
                }

                distancePairs.push_back({ std::abs(nextNode->x - currentNode->x), nextNodeId });
            }

            std::sort(distancePairs.begin(), distancePairs.end());
            if (!distancePairs.empty()) {
                currentNode->nextNodeIds.push_back(distancePairs[0].second);
            }

            if (distancePairs.size() > 1) {
                std::uniform_int_distribution<int> branchChance(0, 99);
                if (branchChance(rng) < 35) {
                    currentNode->nextNodeIds.push_back(distancePairs[1].second);
                }
            }
        }

        for (int nextNodeId : nextFloorNodes) {
            bool hasIncomingEdge = false;
            for (int currentNodeId : currentFloorNodes) {
                RunNodeState* currentNode = FindNodeById(run, currentNodeId);
                if (currentNode == nullptr) {
                    continue;
                }

                if (std::find(currentNode->nextNodeIds.begin(), currentNode->nextNodeIds.end(), nextNodeId) != currentNode->nextNodeIds.end()) {
                    hasIncomingEdge = true;
                    break;
                }
            }

            if (hasIncomingEdge) {
                continue;
            }

            const RunNodeState* nextNode = FindNodeById(run, nextNodeId);
            if (nextNode == nullptr) {
                continue;
            }

            int bestCurrentNodeId = currentFloorNodes.front();
            int bestDistance = 9999;
            for (int currentNodeId : currentFloorNodes) {
                const RunNodeState* currentNode = FindNodeById(run, currentNodeId);
                if (currentNode == nullptr) {
                    continue;
                }

                const int distance = std::abs(nextNode->x - currentNode->x);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestCurrentNodeId = currentNodeId;
                }
            }

            RunNodeState* bestCurrentNode = FindNodeById(run, bestCurrentNodeId);
            if (bestCurrentNode != nullptr) {
                bestCurrentNode->nextNodeIds.push_back(nextNodeId);
            }
        }
    }
}

} // namespace

std::string RunNodeTypeToString(RunNodeType type) {
    switch (type) {
    case RunNodeType::Battle:   return "Battle";
    case RunNodeType::Elite:    return "Elite";
    case RunNodeType::Boss:     return "Boss";
    case RunNodeType::Shop:     return "Shop";
    case RunNodeType::Rest:     return "Rest";
    case RunNodeType::Treasure: return "Treasure";
    case RunNodeType::Event:    return "Event";
    default:                    return "Unknown";
    }
}

std::string RunNodeTypeToDisplayName(RunNodeType type) {
    switch (type) {
    case RunNodeType::Battle:   return u8"일반 전투";
    case RunNodeType::Elite:    return u8"엘리트";
    case RunNodeType::Boss:     return u8"보스";
    case RunNodeType::Shop:     return u8"상점";
    case RunNodeType::Rest:     return u8"휴식";
    case RunNodeType::Treasure: return u8"보물";
    case RunNodeType::Event:    return u8"미지 이벤트";
    default:                    return u8"노드";
    }
}

std::string RunNodeTypeToDescription(RunNodeType type) {
    switch (type) {
    case RunNodeType::Battle:   return u8"일반 적과 마주칩니다.";
    case RunNodeType::Elite:    return u8"강한 적을 상대하고 좋은 보상을 기대할 수 있습니다.";
    case RunNodeType::Boss:     return u8"막의 최종 전투입니다.";
    case RunNodeType::Shop:     return u8"골드로 카드, 포션, 유물을 구매하거나 카드를 제거할 수 있습니다.";
    case RunNodeType::Rest:     return u8"체력을 회복하거나 카드를 강화할 수 있는 모닥불입니다.";
    case RunNodeType::Treasure: return u8"유물이나 골드 등 즉시 보상을 고르는 방입니다.";
    case RunNodeType::Event:    return u8"선택에 따라 결과가 갈라지는 사건이 발생합니다.";
    default:                    return u8"준비 중입니다.";
    }
}

std::string RunNodeResultToString(RunNodeResultType result) {
    switch (result) {
    case RunNodeResultType::Victory:   return "Victory";
    case RunNodeResultType::Defeat:    return "Defeat";
    case RunNodeResultType::Escape:    return "Escape";
    case RunNodeResultType::Abandoned: return "Abandoned";
    case RunNodeResultType::Resolved:  return "Resolved";
    case RunNodeResultType::None:
    default:
        return "None";
    }
}

std::string BuildTimestampText() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime = {};
#ifdef _WIN32
    localtime_s(&localTime, &rawTime);
#else
    localTime = *std::localtime(&rawTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::vector<CardPackOption> BuildStarterCardPacks() {
    std::vector<CardPackOption> packs;

    CardPackOption bleedPack = {};
    bleedPack.id = 0;
    bleedPack.title = u8"핏빛 맹공";
    bleedPack.description = u8"공격 카드와 취약 부여로 템포를 먼저 쥐는 패키지입니다.";
    bleedPack.accentColor = COLOR_RED;
    bleedPack.cards = {
        MakeCard(1000, u8"강타+", 1, u8"적에게 8 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 8, 0),
        MakeCard(1001, u8"강타", 1, u8"적에게 6 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 6, 0),
        MakeCard(1002, u8"악점 노출", 2, u8"적에게 취약 2를 부여합니다.", CardType::Skill, CardTargetType::Enemy, CardEffectType::ApplyVulnerable, CardDiscardEffectType::None, 2, 0),
        MakeCard(1003, u8"재정비", 0, u8"버릴 때 카드 1장을 뽑고 에너지 1을 얻습니다.", CardType::Skill, CardTargetType::None, CardEffectType::None, CardDiscardEffectType::DrawCardsGainEnergy, 1, 1),
        MakeCard(1004, u8"분쇄 타격", 2, u8"적에게 12 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 12, 0)
    };

    CardPackOption shieldPack = {};
    shieldPack.id = 1;
    shieldPack.title = u8"강철 방벽";
    shieldPack.description = u8"안정적인 방어와 손패 정리용 카드가 묶인 패키지입니다.";
    shieldPack.accentColor = COLOR_BLUE;
    shieldPack.cards = {
        MakeCard(1010, u8"수비+", 1, u8"방어도 7을 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 7, 0),
        MakeCard(1011, u8"수비", 1, u8"방어도 5를 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 5, 0),
        MakeCard(1012, u8"굳건한 자세", 2, u8"방어도 10을 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 10, 0),
        MakeCard(1013, u8"재정비", 0, u8"버릴 때 카드 1장을 뽑고 에너지 1을 얻습니다.", CardType::Skill, CardTargetType::None, CardEffectType::None, CardDiscardEffectType::DrawCardsGainEnergy, 1, 1),
        MakeCard(1014, u8"강타", 1, u8"적에게 6 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 6, 0)
    };

    CardPackOption venomPack = {};
    venomPack.id = 2;
    venomPack.title = u8"맹독 실험";
    venomPack.description = u8"취약과 유틸을 활용해 상황을 비트는 패키지입니다.";
    venomPack.accentColor = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    venomPack.cards = {
        MakeCard(1020, u8"악점 노출", 2, u8"적에게 취약 2를 부여합니다.", CardType::Skill, CardTargetType::Enemy, CardEffectType::ApplyVulnerable, CardDiscardEffectType::None, 2, 0),
        MakeCard(1021, u8"강타", 1, u8"적에게 6 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 6, 0),
        MakeCard(1022, u8"수비", 1, u8"방어도 5를 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 5, 0),
        MakeCard(1023, u8"재정비", 0, u8"버릴 때 카드 1장을 뽑고 에너지 1을 얻습니다.", CardType::Skill, CardTargetType::None, CardEffectType::None, CardDiscardEffectType::DrawCardsGainEnergy, 1, 1),
        MakeCard(1024, u8"휘몰아치기", 1, u8"적에게 7 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 7, 0)
    };

    packs.push_back(bleedPack);
    packs.push_back(shieldPack);
    packs.push_back(venomPack);
    return packs;
}

void CreateNewRun(RunStateData& run, std::uint32_t seed, int screenWidth, int screenHeight) {
    run = {};
    run.seed = seed;
    run.gold = 99;
    run.playTimeSec = 0;
    run.finished = false;
    run.won = false;
    run.scene = RunSceneType::CardPackSelect;
    run.overlay = RunOverlayType::None;
    run.pendingConfirm = ConfirmActionType::None;
    run.roomResolved = false;
    run.currentRoomType = RunNodeType::Battle;
    run.currentRoomResult = RunNodeResultType::None;
    run.player = { 0, u8"아이언클래드", 80, 80, 0, 0, 0, 0, 0 };
    run.playerName = run.player.name;
    run.selectedStarterPackIndex = -1;
    run.nodeEntrySnapshot = {};
    run.currentRoomSummaryTitle.clear();
    run.currentRoomSummaryText.clear();
    run.relics.push_back(MakeRelic(1, u8"불타는 피", u8"전투 종료 후 체력을 6 회복합니다."));
    run.potions.push_back(MakePotion(2, u8"회복 포션", u8"체력을 소량 회복합니다.", false));

    GenerateRunMap(run, screenWidth, screenHeight);
    ResetRoomRuntimeState(run);
}

void ApplyStarterPack(RunStateData& run, const CardPackOption& pack) {
    run.selectedCardPackTitle = pack.title;
    for (const CardData& card : pack.cards) {
        run.deck.push_back(card);
    }
}

void ResetRoomRuntimeState(RunStateData& run) {
    run.battleRoom = {};
    run.shopRoom = {};
    run.restRoom = {};
    run.treasureRoom = {};
    run.eventRoom = {};
    run.currentRoomSummaryTitle.clear();
    run.currentRoomSummaryText.clear();
}

void PrepareCurrentRoomState(RunStateData& run) {
    switch (run.currentRoomType) {
    case RunNodeType::Battle:
    case RunNodeType::Elite:
    case RunNodeType::Boss:
        InitializeBattleRoom(run);
        break;
    case RunNodeType::Shop:
        InitializeShopRoom(run);
        break;
    case RunNodeType::Rest:
        InitializeRestRoom(run);
        break;
    case RunNodeType::Treasure:
        InitializeTreasureRoom(run);
        break;
    case RunNodeType::Event:
        InitializeEventRoom(run);
        break;
    default:
        break;
    }
}

void CaptureNodeEntrySnapshot(RunStateData& run) {
    run.nodeEntrySnapshot = {};
    if (run.currentNodeId < 0) {
        return;
    }

    run.nodeEntrySnapshot.valid = true;
    run.nodeEntrySnapshot.nodeId = run.currentNodeId;
    run.nodeEntrySnapshot.floor = run.currentFloor;
    run.nodeEntrySnapshot.player = run.player;
    run.nodeEntrySnapshot.gold = run.gold;
    run.nodeEntrySnapshot.deck = run.deck;
    run.nodeEntrySnapshot.relics = run.relics;
    run.nodeEntrySnapshot.potions = run.potions;
    run.nodeEntrySnapshot.visitedNodeTypes = run.visitedNodeTypes;
}

void RestoreNodeEntrySnapshot(RunStateData& run) {
    if (!run.nodeEntrySnapshot.valid) {
        return;
    }

    run.player = run.nodeEntrySnapshot.player;
    run.gold = run.nodeEntrySnapshot.gold;
    run.deck = run.nodeEntrySnapshot.deck;
    run.relics = run.nodeEntrySnapshot.relics;
    run.potions = run.nodeEntrySnapshot.potions;
    run.visitedNodeTypes = run.nodeEntrySnapshot.visitedNodeTypes;
    run.currentFloor = run.nodeEntrySnapshot.floor;
}

RunNodeState* FindNodeById(RunStateData& run, int nodeId) {
    for (RunNodeState& node : run.nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

const RunNodeState* FindNodeById(const RunStateData& run, int nodeId) {
    for (const RunNodeState& node : run.nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

bool CanEnterNode(const RunStateData& run, int nodeId) {
    const RunNodeState* node = FindNodeById(run, nodeId);
    if (node == nullptr) {
        return false;
    }

    if (!node->unlocked || node->completed) {
        return false;
    }

    if (run.currentNodeId < 0) {
        return node->floor == 1;
    }

    if (!run.roomResolved) {
        return false;
    }

    const RunNodeState* currentNode = FindNodeById(run, run.currentNodeId);
    if (currentNode == nullptr) {
        return false;
    }

    return std::find(currentNode->nextNodeIds.begin(), currentNode->nextNodeIds.end(), nodeId) != currentNode->nextNodeIds.end();
}

bool EnterNode(RunStateData& run, int nodeId) {
    if (!CanEnterNode(run, nodeId)) {
        return false;
    }

    for (RunNodeState& node : run.nodes) {
        node.isCurrent = false;
    }

    RunNodeState* node = FindNodeById(run, nodeId);
    if (node == nullptr) {
        return false;
    }

    node->isCurrent = true;
    node->visited = true;
    run.currentNodeId = node->id;
    run.currentFloor = node->floor;
    run.currentRoomType = node->type;
    run.currentRoomResult = RunNodeResultType::None;
    run.roomResolved = false;
    run.scene = RunSceneType::Room;
    run.overlay = RunOverlayType::None;
    run.visitedNodeTypes.push_back(node->type);
    ResetRoomRuntimeState(run);
    CaptureNodeEntrySnapshot(run);
    PrepareCurrentRoomState(run);
    return true;
}

void UnlockNextNodes(RunStateData& run, int nodeId) {
    const RunNodeState* currentNode = FindNodeById(run, nodeId);
    if (currentNode == nullptr) {
        return;
    }

    for (int nextNodeId : currentNode->nextNodeIds) {
        RunNodeState* nextNode = FindNodeById(run, nextNodeId);
        if (nextNode != nullptr) {
            nextNode->unlocked = true;
        }
    }
}

void ResolveCurrentNode(RunStateData& run, RunNodeResultType result) {
    RunNodeState* node = FindNodeById(run, run.currentNodeId);
    if (node == nullptr) {
        return;
    }

    node->completed = true;
    node->result = result;
    node->isCurrent = false;
    run.currentRoomResult = result;
    run.roomResolved = true;
    run.nodeEntrySnapshot = {};

    if (result == RunNodeResultType::Victory || result == RunNodeResultType::Resolved || result == RunNodeResultType::Escape) {
        UnlockNextNodes(run, node->id);
    }
}

void ReopenCurrentNodeIntro(RunStateData& run) {
    if (run.currentNodeId < 0) {
        return;
    }

    RunNodeState* node = FindNodeById(run, run.currentNodeId);
    if (node == nullptr) {
        return;
    }

    node->completed = false;
    node->result = RunNodeResultType::None;
    node->isCurrent = true;
    run.currentRoomType = node->type;
    run.currentRoomResult = RunNodeResultType::None;
    run.roomResolved = false;
    run.scene = RunSceneType::Room;
    run.overlay = RunOverlayType::None;
    ResetRoomRuntimeState(run);
    PrepareCurrentRoomState(run);
}

RunRecordData BuildRunRecord(const RunStateData& run, bool won, const std::string& failureReasonText) {
    RunRecordData record = {};
    record.won = won;
    record.seed = run.seed;
    record.reachedFloor = run.currentFloor;
    record.playTimeSec = run.playTimeSec;
    record.timestampText = BuildTimestampText();
    record.failureReasonText = failureReasonText;
    record.deckSnapshot = run.deck;
    record.relics = run.relics;
    record.visitedNodes = run.visitedNodeTypes;
    return record;
}
