// -----------------------------------------------------------------------------
// @file       RunState.cpp
// @brief      런 생성, 방 상태 준비, 맵 생성 구현부
// -----------------------------------------------------------------------------
#include "RunState.h"
#include "CardLibrary.h"
#include "ScreenManager.h"

#include <algorithm>
#include <array>
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
    return CardLibrary::BuildGeneralCardPool();
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

// 각 비전투 방은 "처음 진입할 때 1회 초기화"하는 방식으로 유지한다.
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
    run.shopRoom.noticeText = u8"상인을 눌러 물건을 살펴보십시오.";
    run.shopRoom.uiOpen = false;
    run.shopRoom.chatterIndex = 0;
    run.shopRoom.chatterTimerSec = 0.0f;

    std::vector<CardData> cardOffers;
    std::vector<CardData> favoredPool = CardLibrary::BuildArchetypeRewardPool(run.selectedCardPackArchetype);
    std::vector<CardData> fallbackPool = cardPool;

    auto eraseChosenFromPool = [](std::vector<CardData>& pool, int cardId) {
        pool.erase(
            std::remove_if(
                pool.begin(),
                pool.end(),
                [cardId](const CardData& card) { return card.id == cardId; }),
            pool.end());
    };

    const int guaranteedFavoredCount = (run.selectedCardPackArchetype == CardArchetype::None) ? 0 : 2;
    for (int index = 0; index < guaranteedFavoredCount && !favoredPool.empty(); ++index) {
        const CardData favoredCard = PickFromPool(favoredPool, rng);
        cardOffers.push_back(favoredCard);
        eraseChosenFromPool(favoredPool, favoredCard.id);
        eraseChosenFromPool(fallbackPool, favoredCard.id);
    }

    while (static_cast<int>(cardOffers.size()) < 7 && !fallbackPool.empty()) {
        const CardData pickedCard = PickFromPool(fallbackPool, rng);
        cardOffers.push_back(pickedCard);
        eraseChosenFromPool(fallbackPool, pickedCard.id);
    }

    for (size_t index = 0; index < cardOffers.size(); ++index) {
        ShopOfferState offer = {};
        offer.id = 5000 + static_cast<int>(index);
        offer.type = ShopOfferType::Card;
        offer.card = cardOffers[index];
        offer.title = cardOffers[index].name;
        offer.description = cardOffers[index].description;
        offer.price = 45 + (cardOffers[index].cost * 15) + (cardOffers[index].rarity == CardRarity::Rare ? 40 : (cardOffers[index].rarity == CardRarity::Uncommon ? 15 : 0));
        run.shopRoom.offers.push_back(offer);
    }

    const std::vector<RelicData> relicOffers = PickDistinctFromPool(relicPool, rng, 3);
    for (size_t index = 0; index < relicOffers.size(); ++index) {
        ShopOfferState relicOffer = {};
        relicOffer.id = 5100 + static_cast<int>(index);
        relicOffer.type = ShopOfferType::Relic;
        relicOffer.relic = relicOffers[index];
        relicOffer.title = relicOffers[index].name;
        relicOffer.description = relicOffers[index].description;
        relicOffer.price = 140 + static_cast<int>(index) * 10;
        run.shopRoom.offers.push_back(relicOffer);
    }

    const std::vector<PotionData> potionOffers = PickDistinctFromPool(potionPool, rng, 3);
    for (size_t index = 0; index < potionOffers.size(); ++index) {
        ShopOfferState potionOffer = {};
        potionOffer.id = 5200 + static_cast<int>(index);
        potionOffer.type = ShopOfferType::Potion;
        potionOffer.potion = potionOffers[index];
        potionOffer.title = potionOffers[index].name;
        potionOffer.description = potionOffers[index].description;
        potionOffer.price = 50 + static_cast<int>(index) * 10;
        run.shopRoom.offers.push_back(potionOffer);
    }
}

EntityData BuildEnemyTemplateForRoom(RunNodeType type) {
    switch (type) {
    case RunNodeType::Elite:
        return { 9100, u8"수호 슬라임", 72, 72, 0, 1, 0, 0, 0, 0 };
    case RunNodeType::Boss:
        return { 9200, u8"수호자 프로토타입", 130, 130, 0, 2, 0, 0, 0, 0 };
    case RunNodeType::Battle:
    default:
        return { 9000, u8"훈련용 슬라임", 48, 48, 0, 0, 0, 0, 0, 0 };
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
    run.treasureRoom.introText = u8"상자를 열면 골드와 유물, 때로는 포션까지 챙길 수 있습니다.";
    run.treasureRoom.noticeText = u8"상자를 눌러 내용을 확인하십시오.";
    run.treasureRoom.chestOpened = false;
    run.treasureRoom.choiceCommitted = false;
    run.treasureRoom.selectedChoiceId = -1;
    run.treasureRoom.goldReward = 70 + static_cast<int>(rng() % 31);
    run.treasureRoom.relicRewards = PickDistinctFromPool(relicPool, rng, 2);

    if (run.potions.size() < kMaxPotionCount) {
        std::uniform_int_distribution<int> potionRoll(1, 100);
        if (potionRoll(rng) <= 45) {
            run.treasureRoom.potionRewardAvailable = true;
            run.treasureRoom.potionReward = PickFromPool(potionPool, rng);
        }
    }

    BattleRewardState& rewards = run.treasureRoom.rewards;
    rewards = {};
    rewards.active = true;
    rewards.title = u8"보물";
    rewards.message = u8"상자에서 꺼낸 전리품을 챙긴 뒤 계속 전진하십시오.";
    rewards.goldAvailable = true;
    rewards.goldAmount = run.treasureRoom.goldReward;
    rewards.potionAvailable = run.treasureRoom.potionRewardAvailable;
    rewards.potion = run.treasureRoom.potionReward;
    rewards.relicRewards = run.treasureRoom.relicRewards;
    rewards.relicClaimed.assign(rewards.relicRewards.size(), 0);
    rewards.cardRewardAvailable = false;
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

struct LaneSegment {
    int fromLane = 0;
    int toLane = 0;
};

struct LaneFlowState {
    int lane = 0;
    int lastDelta = 0;
    int straightStreak = 0;
};

struct LaneCandidate {
    int nextLane = 0;
    int delta = 0;
    int weight = 0;
};

bool HasDuplicateSegment(const std::vector<LaneSegment>& segments, int fromLane, int toLane) {
    for (const LaneSegment& segment : segments) {
        if (segment.fromLane == fromLane && segment.toLane == toLane) {
            return true;
        }
    }
    return false;
}

bool WouldCrossExistingSegment(const std::vector<LaneSegment>& segments, int fromLane, int toLane) {
    for (const LaneSegment& segment : segments) {
        const bool crosses =
            (fromLane < segment.fromLane && toLane > segment.toLane) ||
            (fromLane > segment.fromLane && toLane < segment.toLane);
        if (crosses) {
            return true;
        }
    }
    return false;
}

int CountIncomingSegments(const std::vector<LaneSegment>& segments, int toLane) {
    int count = 0;
    for (const LaneSegment& segment : segments) {
        if (segment.toLane == toLane) {
            ++count;
        }
    }
    return count;
}

int CountOutgoingSegments(const std::vector<LaneSegment>& segments, int fromLane) {
    int count = 0;
    for (const LaneSegment& segment : segments) {
        if (segment.fromLane == fromLane) {
            ++count;
        }
    }
    return count;
}

int CountUniqueTargetLanes(const std::vector<LaneSegment>& segments) {
    std::array<bool, 7> used = {};
    int count = 0;
    for (const LaneSegment& segment : segments) {
        if (!used[static_cast<size_t>(segment.toLane)]) {
            used[static_cast<size_t>(segment.toLane)] = true;
            ++count;
        }
    }
    return count;
}

bool CreatesWideConvergence(const std::vector<LaneSegment>& segments, int fromLane, int toLane) {
    for (const LaneSegment& segment : segments) {
        if (segment.toLane != toLane) {
            continue;
        }

        if (std::abs(segment.fromLane - fromLane) >= 2) {
            return true;
        }
    }
    return false;
}

int PickWeightedLane(const std::vector<LaneCandidate>& candidates, std::mt19937& rng) {
    if (candidates.empty()) {
        return 0;
    }

    int totalWeight = 0;
    for (const LaneCandidate& candidate : candidates) {
        totalWeight += candidate.weight;
    }

    if (totalWeight <= 0) {
        return candidates.front().nextLane;
    }

    std::uniform_int_distribution<int> pickDist(1, totalWeight);
    int pickedWeight = pickDist(rng);
    for (const LaneCandidate& candidate : candidates) {
        pickedWeight -= candidate.weight;
        if (pickedWeight <= 0) {
            return candidate.nextLane;
        }
    }

    return candidates.back().nextLane;
}

std::vector<LaneCandidate> BuildLaneCandidates(
    const LaneFlowState& state,
    const std::vector<LaneSegment>& floorSegments,
    bool preferFreshTarget,
    bool allowCrowdedTarget,
    bool allowWideConvergence) {
    std::vector<LaneCandidate> candidates;

    for (int delta = -1; delta <= 1; ++delta) {
        const int nextLane = state.lane + delta;
        if (nextLane < 0 || nextLane >= 7) {
            continue;
        }
        if (WouldCrossExistingSegment(floorSegments, state.lane, nextLane)) {
            continue;
        }
        if (HasDuplicateSegment(floorSegments, state.lane, nextLane)) {
            continue;
        }
        if (!allowCrowdedTarget && CountIncomingSegments(floorSegments, nextLane) >= 2) {
            continue;
        }
        if (!allowWideConvergence && CreatesWideConvergence(floorSegments, state.lane, nextLane)) {
            continue;
        }

        int weight = 0;
        switch (std::abs(delta)) {
        case 0: weight = 18; break;
        case 1: weight = 28; break;
        case 2: weight = 6; break;
        default: break;
        }

        if (state.straightStreak >= 2 && delta == 0) {
            weight = 4;
        }
        if (state.straightStreak >= 1 && std::abs(delta) == 1) {
            weight += 10;
        }
        if (state.lastDelta == 0 && std::abs(delta) == 1) {
            weight += 5;
        }
        if (state.lastDelta != 0 && delta == state.lastDelta && std::abs(delta) == 1) {
            weight += 5;
        }
        if (std::abs(state.lastDelta) == 2 && std::abs(delta) == 2) {
            weight = (std::max)(1, weight / 3);
        }

        const int incomingCount = CountIncomingSegments(floorSegments, nextLane);
        if (preferFreshTarget) {
            if (incomingCount == 0) {
                weight += 12;
            }
            else {
                weight = (std::max)(1, weight / 3);
            }
        }
        else if (incomingCount > 0) {
            weight += 6;
        }

        candidates.push_back({ nextLane, delta, weight });
    }

    return candidates;
}

int ChooseTargetNodeCount(int floor, int regularFloorCount, int currentNodeCount, std::mt19937& rng) {
    std::array<int, 5> offsets = { -1, 0, 0, 0, 1 };
    std::uniform_int_distribution<int> offsetDist(0, static_cast<int>(offsets.size()) - 1);
    const int offset = offsets[static_cast<size_t>(offsetDist(rng))];

    int target = currentNodeCount + offset;
    if (floor <= 2) {
        target = (std::max)(target, 4);
    }
    if (floor >= regularFloorCount - 2) {
        target = (std::min)(target, 4);
    }

    target = (std::max)(3, (std::min)(5, target));
    return target;
}

RunNodeType PickNodeTypeForFloor(int floor, int regularFloorCount, std::mt19937& rng) {
    if (floor <= 1) {
        return RunNodeType::Battle;
    }
    if (floor == regularFloorCount - 1) {
        return RunNodeType::Treasure;
    }
    if (floor == regularFloorCount) {
        return RunNodeType::Rest;
    }

    std::uniform_int_distribution<int> dist(0, 99);
    const int roll = dist(rng);

    if (floor <= 3) {
        if (roll < 55) return RunNodeType::Battle;
        if (roll < 78) return RunNodeType::Event;
        return RunNodeType::Shop;
    }

    if (floor <= 5) {
        if (roll < 38) return RunNodeType::Battle;
        if (roll < 58) return RunNodeType::Event;
        if (roll < 74) return RunNodeType::Shop;
        if (roll < 88) return RunNodeType::Treasure;
        return RunNodeType::Rest;
    }

    if (roll < 30) return RunNodeType::Battle;
    if (roll < 47) return RunNodeType::Event;
    if (roll < 60) return RunNodeType::Shop;
    if (roll < 74) return RunNodeType::Rest;
    if (roll < 88) return RunNodeType::Treasure;
    return RunNodeType::Elite;
}

// 맵 생성은 층별 노드를 먼저 두고, 그 다음 레인 연결을 만들어 흐름을 정리한다.
void GenerateRunMap(RunStateData& run, int screenWidth, int screenHeight) {
    std::mt19937 rng(run.seed);

    constexpr int kLaneCount = 7;
    constexpr int kRegularFloorCount = 9;
    constexpr int kStartNodeCount = 4;
    const std::array<int, kStartNodeCount> startLanes = { 0, 2, 4, 6 };

    run.totalFloors = kRegularFloorCount + 1;
    run.currentFloor = 0;
    run.currentNodeId = -1;
    run.nodes.clear();
    run.visitedNodeTypes.clear();

    std::vector<std::vector<int>> floorNodeIds;
    floorNodeIds.resize(static_cast<size_t>(run.totalFloors + 1));
    std::vector<std::vector<int>> nodeIdByFloorLane(static_cast<size_t>(kRegularFloorCount + 1), std::vector<int>(kLaneCount, -1));
    std::vector<std::vector<bool>> occupied(static_cast<size_t>(kRegularFloorCount + 1), std::vector<bool>(kLaneCount, false));
    std::vector<std::vector<LaneSegment>> floorSegments(static_cast<size_t>(kRegularFloorCount));

    int nextId = 0;
    const int horizontalMargin = (std::max)(26, (screenWidth * 15) / 100);
    const int centralInset = (std::max)(8, (screenWidth * 6) / 100);
    const int minLaneGap = 8;
    const int floorSpacing = 19;
    const int bottomY = screenHeight - 12;
    const int minX = horizontalMargin;
    const int maxX = screenWidth - horizontalMargin - 1;
    const int generationMinX = minX + centralInset;
    const int generationMaxX = maxX - centralInset;
    const float laneSpacing = static_cast<float>(generationMaxX - generationMinX) / static_cast<float>(kLaneCount - 1);
    std::uniform_int_distribution<int> xJitterDist(-4, 4);
    std::uniform_int_distribution<int> yJitterDist(-2, 2);

    std::vector<LaneFlowState> activeNodes;
    activeNodes.reserve(kStartNodeCount);
    for (int lane : startLanes) {
        activeNodes.push_back({ lane, 0, 0 });
        occupied[1][lane] = true;
    }

    for (int floor = 1; floor < kRegularFloorCount; ++floor) {
        std::vector<LaneSegment>& segments = floorSegments[static_cast<size_t>(floor)];
        const int targetNodeCount = ChooseTargetNodeCount(floor + 1, kRegularFloorCount, static_cast<int>(activeNodes.size()), rng);

        std::vector<int> processOrder(activeNodes.size(), 0);
        for (int index = 0; index < static_cast<int>(activeNodes.size()); ++index) {
            processOrder[static_cast<size_t>(index)] = index;
            occupied[static_cast<size_t>(floor)][activeNodes[static_cast<size_t>(index)].lane] = true;
        }
        std::shuffle(processOrder.begin(), processOrder.end(), rng);

        std::array<LaneFlowState, kLaneCount> nextStates = {};
        std::array<bool, kLaneCount> nextStateExists = {};

        auto registerSegment = [&](const LaneFlowState& fromState, int nextLane) {
            segments.push_back({ fromState.lane, nextLane });
            occupied[static_cast<size_t>(floor + 1)][nextLane] = true;

            const int delta = nextLane - fromState.lane;
            LaneFlowState nextState = {};
            nextState.lane = nextLane;
            nextState.lastDelta = delta;
            nextState.straightStreak = (delta == 0) ? (fromState.straightStreak + 1) : 0;

            if (!nextStateExists[static_cast<size_t>(nextLane)]) {
                nextStates[static_cast<size_t>(nextLane)] = nextState;
                nextStateExists[static_cast<size_t>(nextLane)] = true;
                return;
            }

            LaneFlowState& existingState = nextStates[static_cast<size_t>(nextLane)];
            if (nextState.straightStreak < existingState.straightStreak ||
                std::abs(nextState.lastDelta) < std::abs(existingState.lastDelta)) {
                existingState = nextState;
            }
        };

        for (int orderIndex = 0; orderIndex < static_cast<int>(processOrder.size()); ++orderIndex) {
            const LaneFlowState& state = activeNodes[static_cast<size_t>(processOrder[static_cast<size_t>(orderIndex)])];
            const int uniqueTargetCount = CountUniqueTargetLanes(segments);
            const int remainingSources = static_cast<int>(processOrder.size()) - orderIndex;
            const bool preferFreshTarget = (uniqueTargetCount + remainingSources) <= targetNodeCount;

            std::vector<LaneCandidate> candidates = BuildLaneCandidates(state, segments, preferFreshTarget, false, false);
            if (candidates.empty()) {
                candidates = BuildLaneCandidates(state, segments, preferFreshTarget, true, false);
            }
            if (candidates.empty()) {
                candidates = BuildLaneCandidates(state, segments, false, true, true);
            }
            if (candidates.empty()) {
                candidates.push_back({ state.lane, 0, 1 });
            }

            registerSegment(state, PickWeightedLane(candidates, rng));
        }

        while (CountUniqueTargetLanes(segments) < targetNodeCount) {
            bool addedBranch = false;
            std::shuffle(processOrder.begin(), processOrder.end(), rng);

            for (int stateIndex : processOrder) {
                const LaneFlowState& state = activeNodes[static_cast<size_t>(stateIndex)];
                if (CountOutgoingSegments(segments, state.lane) >= 2) {
                    continue;
                }

                std::vector<LaneCandidate> branchCandidates = BuildLaneCandidates(state, segments, true, false, false);
                if (branchCandidates.empty()) {
                    branchCandidates = BuildLaneCandidates(state, segments, true, true, false);
                }
                if (branchCandidates.empty()) {
                    continue;
                }

                registerSegment(state, PickWeightedLane(branchCandidates, rng));
                addedBranch = true;
                break;
            }

            if (!addedBranch) {
                break;
            }
        }

        activeNodes.clear();
        for (int lane = 0; lane < kLaneCount; ++lane) {
            if (nextStateExists[static_cast<size_t>(lane)]) {
                activeNodes.push_back(nextStates[static_cast<size_t>(lane)]);
            }
        }
    }

    for (int floor = 1; floor <= kRegularFloorCount; ++floor) {
        const int baseY = bottomY - ((floor - 1) * floorSpacing);
        std::array<int, kLaneCount> floorLaneXs = {};

        for (int lane = 0; lane < kLaneCount; ++lane) {
            const int baseX = generationMinX + static_cast<int>(std::round(static_cast<float>(lane) * laneSpacing));
            const int jitter = (floor == 1 || floor == kRegularFloorCount) ? 0 : xJitterDist(rng);
            floorLaneXs[static_cast<size_t>(lane)] = baseX + jitter;
        }

        for (int lane = 1; lane < kLaneCount; ++lane) {
            floorLaneXs[static_cast<size_t>(lane)] = (std::max)(
                floorLaneXs[static_cast<size_t>(lane)],
                floorLaneXs[static_cast<size_t>(lane - 1)] + minLaneGap);
        }

        floorLaneXs[static_cast<size_t>(kLaneCount - 1)] = (std::min)(
            floorLaneXs[static_cast<size_t>(kLaneCount - 1)],
            maxX);

        for (int lane = kLaneCount - 2; lane >= 0; --lane) {
            floorLaneXs[static_cast<size_t>(lane)] = (std::min)(
                floorLaneXs[static_cast<size_t>(lane)],
                floorLaneXs[static_cast<size_t>(lane + 1)] - minLaneGap);
        }

        floorLaneXs[0] = (std::max)(floorLaneXs[0], minX);

        for (int lane = 0; lane < kLaneCount; ++lane) {
            if (!occupied[static_cast<size_t>(floor)][lane]) {
                continue;
            }

            RunNodeState node = {};
            node.id = nextId++;
            node.floor = floor;
            node.x = floorLaneXs[static_cast<size_t>(lane)];
            node.y = baseY + ((floor == 1 || floor == kRegularFloorCount) ? 0 : yJitterDist(rng));
            node.type = PickNodeTypeForFloor(floor, kRegularFloorCount, rng);
            node.unlocked = (floor == 1);
            node.reachable = (floor == 1);

            nodeIdByFloorLane[static_cast<size_t>(floor)][lane] = node.id;
            floorNodeIds[static_cast<size_t>(floor)].push_back(node.id);
            run.nodes.push_back(node);
        }
    }

    RunNodeState bossNode = {};
    bossNode.id = nextId++;
    bossNode.floor = run.totalFloors;
    int topFloorSumX = 0;
    int topFloorCount = 0;
    for (int topNodeId : floorNodeIds[static_cast<size_t>(kRegularFloorCount)]) {
        const RunNodeState* topNode = FindNodeById(run, topNodeId);
        if (topNode != nullptr) {
            topFloorSumX += topNode->x;
            ++topFloorCount;
        }
    }
    bossNode.x = (topFloorCount > 0) ? (topFloorSumX / topFloorCount) : ((minX + maxX) / 2);
    bossNode.y = bottomY - (kRegularFloorCount * floorSpacing) - 4;
    bossNode.type = RunNodeType::Boss;
    floorNodeIds[static_cast<size_t>(run.totalFloors)].push_back(bossNode.id);
    run.nodes.push_back(bossNode);

    for (int floor = 1; floor < kRegularFloorCount; ++floor) {
        for (const LaneSegment& segment : floorSegments[static_cast<size_t>(floor)]) {
            const int fromNodeId = nodeIdByFloorLane[static_cast<size_t>(floor)][segment.fromLane];
            const int toNodeId = nodeIdByFloorLane[static_cast<size_t>(floor + 1)][segment.toLane];
            if (fromNodeId < 0 || toNodeId < 0) {
                continue;
            }

            RunNodeState* fromNode = FindNodeById(run, fromNodeId);
            if (fromNode != nullptr &&
                std::find(fromNode->nextNodeIds.begin(), fromNode->nextNodeIds.end(), toNodeId) == fromNode->nextNodeIds.end()) {
                fromNode->nextNodeIds.push_back(toNodeId);
            }
        }
    }

    for (int topNodeId : floorNodeIds[static_cast<size_t>(kRegularFloorCount)]) {
        RunNodeState* topNode = FindNodeById(run, topNodeId);
        if (topNode != nullptr) {
            topNode->nextNodeIds.push_back(bossNode.id);
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

// 시작 카드팩 후보는 여기에서만 관리한다.
std::vector<CardPackOption> BuildStarterCardPacks() {
    std::vector<CardPackOption> packs;

    CardPackOption comboPack = {};
    comboPack.id = 0;
    comboPack.archetype = CardArchetype::Combo;
    comboPack.title = u8"연타 장전";
    comboPack.description = u8"콤보와 적 시간 지연으로 템포를 틀어쥐는 패키지입니다.";
    comboPack.accentColor = COLOR_RED;
    comboPack.cards = CardLibrary::BuildStarterPackCards(comboPack.archetype);
    packs.push_back(comboPack);

    CardPackOption strengthPack = {};
    strengthPack.id = 1;
    strengthPack.archetype = CardArchetype::Strength;
    strengthPack.title = u8"근력 폭주";
    strengthPack.description = u8"초반에는 준비하고 후반에는 힘으로 밀어붙이는 패키지입니다.";
    strengthPack.accentColor = COLOR_RED | FOREGROUND_INTENSITY;
    strengthPack.cards = CardLibrary::BuildStarterPackCards(strengthPack.archetype);
    packs.push_back(strengthPack);

    CardPackOption blockPack = {};
    blockPack.id = 2;
    blockPack.archetype = CardArchetype::Block;
    blockPack.title = u8"방벽 전개";
    blockPack.description = u8"방어도를 쌓아 피해로 환산하는 실시간 방밀 패키지입니다.";
    blockPack.accentColor = COLOR_BLUE;
    blockPack.cards = CardLibrary::BuildStarterPackCards(blockPack.archetype);
    packs.push_back(blockPack);

    CardPackOption poisonPack = {};
    poisonPack.id = 3;
    poisonPack.archetype = CardArchetype::Poison;
    poisonPack.title = u8"맹독 잠식";
    poisonPack.description = u8"독 누적과 안정적인 방어로 적을 굳혀 죽이는 패키지입니다.";
    poisonPack.accentColor = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    poisonPack.cards = CardLibrary::BuildStarterPackCards(poisonPack.archetype);
    packs.push_back(poisonPack);

    CardPackOption cyclePack = {};
    cyclePack.id = 4;
    cyclePack.archetype = CardArchetype::Cycle;
    cyclePack.title = u8"순환 가속";
    cyclePack.description = u8"드로우, 버리기, 에너지 증폭으로 실시간 난전을 만드는 패키지입니다.";
    cyclePack.accentColor = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    cyclePack.cards = CardLibrary::BuildStarterPackCards(cyclePack.archetype);
    packs.push_back(cyclePack);

    return packs;
}

// 새 런 생성 시 저장 가능한 기본 상태를 한 번에 구성한다.
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
    run.player = { 0, u8"아이언클래드", 80, 80, 0, 0, 0, 0, 0, 0 };
    run.playerName = run.player.name;
    run.selectedCardPackTitle.clear();
    run.selectedCardPackArchetype = CardArchetype::None;
    run.selectedStarterPackIndex = -1;
    run.nodeEntrySnapshot = {};
    run.currentRoomSummaryTitle.clear();
    run.currentRoomSummaryText.clear();
    run.relics.push_back(MakeRelic(1, u8"불타는 피", u8"전투 종료 후 체력을 6 회복합니다."));
    run.potions.push_back(MakePotion(2, u8"회복 포션", u8"체력을 소량 회복합니다.", false));

    GenerateRunMap(run, screenWidth, screenHeight);
    RefreshReachableNodes(run);
    ResetRoomRuntimeState(run);
}

void ApplyStarterPack(RunStateData& run, const CardPackOption& pack) {
    constexpr int kPlayerDeckVisualIdBase = 1000;

    run.deck.clear();
    const std::vector<CardData> baseDeck = CardLibrary::BuildBaseStarterDeck();
    run.deck.insert(run.deck.end(), baseDeck.begin(), baseDeck.end());
    run.selectedCardPackTitle = pack.title;
    run.selectedCardPackArchetype = pack.archetype;
    run.player.id = kPlayerDeckVisualIdBase + static_cast<int>(pack.archetype);
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

// 현재 노드 타입에 따라 필요한 방 런타임 데이터를 지연 초기화한다.
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

    if (node->completed || !node->reachable) {
        return false;
    }

    if (!run.roomResolved) {
        return run.currentNodeId < 0;
    }

    return true;
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
    RefreshReachableNodes(run);
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

void RefreshReachableNodes(RunStateData& run) {
    for (RunNodeState& node : run.nodes) {
        node.reachable = false;
    }

    if (run.currentNodeId < 0) {
        for (RunNodeState& node : run.nodes) {
            if (node.floor == 1 && node.unlocked && !node.completed) {
                node.reachable = true;
            }
        }
        return;
    }

    if (!run.roomResolved) {
        return;
    }

    const RunNodeState* currentNode = FindNodeById(run, run.currentNodeId);
    if (currentNode == nullptr) {
        return;
    }

    for (int nextNodeId : currentNode->nextNodeIds) {
        RunNodeState* nextNode = FindNodeById(run, nextNodeId);
        if (nextNode != nullptr && !nextNode->completed) {
            nextNode->reachable = true;
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
    RefreshReachableNodes(run);
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
    RefreshReachableNodes(run);
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
