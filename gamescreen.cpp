#include "gamescreen.h"

#include <prism/numberpopuphandler.h>

#include "bookscreen.h"

static struct 
{
    int mLevel = 0;
    int mPlayerLoveCount;
    int mStrengthLevel;
    int mSpeedLevel;
    int mGameTicks;
} gGameScreenData;

class GameScreen
{
public:

    double sfxVol = 0.2;

    std::vector<int> enemyStrengths = { 100, 10000, 1000000 };
    std::vector<int> enemyLifes = { 1000, 100000, 10000000 };
    std::vector<int> playerStrengths = { 1, 100, 10000, 1000000 };
    std::vector<int> playerLifes = { 1, 1000, 100000, 10000000 };
    std::vector<int> loveGains = { 10, 1000, 100000 };

    GameScreen() {
        instantiateActor(getPrismNumberPopupHandler());
        load();
        //activateCollisionHandlerDebugMode();
    }

    MugenSpriteFile mSprites;
    MugenAnimations mAnimations;
    MugenSounds mSounds;

    void loadFiles() {
        mSprites = loadMugenSpriteFileWithoutPalette("game/GAME.sff");
        mAnimations = loadMugenAnimationFile("game/GAME.air");
        mSounds = loadMugenSoundFile("game/GAME.snd");
    }

    void load() {
        loadFiles();
        loadGame();
        streamMusicFile("game/GAME.ogg");
    }

    void loadGame() {
        loadCollisionLists();
        loadBG();
        loadWaveStart();
        loadPlayer();
        loadEnemies();
        loadUI();
        loadWinning();
        loadUpgradeScreen();
    }

    void update() {
        updateBG();
        updateWaveStart();
        updatePlayer();
        updateEnemies();
        updateUI();
        updateWinning();
        updateUpgradeScreen();
    }

    // START UI
    MugenAnimationHandlerElement* waveStartUI;
    int waveStartTextId;
    bool hasShownWaveStart = false;
    bool isWaveStartActive = false;
    int waveStartTicks = 0;
    void loadWaveStart() {
        waveStartUI = addMugenAnimation(getMugenAnimation(&mAnimations, 90), &mSprites, Vector3D(0, 0, 40));
        setMugenAnimationVisibility(waveStartUI, 0);
        std::string s = std::string("WAVE ") + std::to_string(gGameScreenData.mLevel + 1);
        waveStartTextId = addMugenTextMugenStyle(s.c_str(), Vector3D(115, 230, 40), Vector3DI(2, 0, 1));
        setMugenTextVisibility(waveStartTextId, 0);
        setMugenTextScale(waveStartTextId, 2.0);
    }
    void updateWaveStart() {
        updateWaveStartStart();
        updateWaveStartActive();
    }
    void updateWaveStartStart() {
        if (hasShownWaveStart) return;
        setMugenAnimationVisibility(waveStartUI, 1);
        setMugenTextVisibility(waveStartTextId, 1);
        isWaveStartActive = true;
        hasShownWaveStart = true;
    }
    void updateWaveStartActive() {
        if (!isWaveStartActive) return;

        waveStartTicks++;
        if (waveStartTicks > 180 || hasPressedStartFlank())
        {
            setMugenAnimationVisibility(lifebarBG, 1);
            setMugenAnimationVisibility(lifebarFG, 1);
            setMugenAnimationVisibility(loveCounter, 1);
            setMugenTextVisibility(loveCounterTextId, 1);
            setMugenAnimationVisibility(waveStartUI, 0);
            setMugenTextVisibility(waveStartTextId, 0);
            isWaveStartActive = false;
        }
    }

    // GENERAL
    CollisionListData* playerCollisionList;
    CollisionListData* playerAttackCollisionList;
    CollisionListData* enemyCollisionList;
    CollisionListData* enemyAttackCollisionList;
    void loadCollisionLists()
    {
        playerCollisionList = addCollisionListToHandler();
        playerAttackCollisionList = addCollisionListToHandler();
        enemyCollisionList = addCollisionListToHandler();
        enemyAttackCollisionList = addCollisionListToHandler();
        addCollisionHandlerCheck(playerAttackCollisionList, enemyCollisionList);
        addCollisionHandlerCheck(playerCollisionList, enemyAttackCollisionList);
    }

    double playerAreaStart = 76;
    double playerAreaEnd = 171;
    double yToZ(double y) {
        double t = (y - playerAreaStart) / (playerAreaEnd - playerAreaStart);
        return 10.f + 10.f * t;
    }
    double yToScale(double y) {
        double t = (y - playerAreaStart) / (playerAreaEnd - playerAreaStart);
        return 0.5 + 0.5f * t;
    }

    // BG
    int bgEntity;
    int girlEntity;
    void loadBG() {
        bgEntity = addBlitzEntity(Vector3D(0, 0, 1));
        addBlitzMugenAnimationComponent(bgEntity, &mSprites, &mAnimations, 1);
        girlEntity = addBlitzEntity(Vector3D(99, 47, 2));
        addBlitzMugenAnimationComponent(girlEntity, &mSprites, &mAnimations, 5);
    }
    void updateBG() {}

    // Player
    int playerEntity;
    int playerAttackCollisionId;
    int playerPassiveCollisionId;
    int playerStrength = 1;
    void loadPlayer() {
        playerEntity = addBlitzEntity(Vector3D(100, 100, 10));
        addBlitzMugenAnimationComponent(playerEntity, &mSprites, &mAnimations, 10);
        playerAttackCollisionId = addBlitzCollisionAttackMugen(playerEntity, playerAttackCollisionList);
        playerPassiveCollisionId = addBlitzCollisionPassiveMugen(playerEntity, playerCollisionList);
        playerStrength = playerStrengths[min(gGameScreenData.mStrengthLevel, int(playerStrengths.size() - 1))];
        maxPlayerLife = playerLifes[min(gGameScreenData.mSpeedLevel, int(playerLifes.size() - 1))];
        playerLife = maxPlayerLife;

        auto playerPosRef = getBlitzEntityPositionReference(playerEntity);
        playerPosRef->z = yToZ(playerPosRef->y);
        setBlitzMugenAnimationBaseDrawScale(playerEntity, yToScale(playerPosRef->y));
    }
    void updatePlayer()
    {
        if (isUpgradeScreenActive || isWaveStartActive || isWinning) return;
        gGameScreenData.mGameTicks++;
        updatePlayerWalking();
        updatePlayerPunching();
        updatePlayerReturningToIdle();
        updatePlayerGettingHit();
        updatePlayerDying();
    }

    void updatePlayerReturningToIdle()
    {
        auto animationNo = getBlitzMugenAnimationAnimationNumber(playerEntity);
        bool isBlocked = (animationNo == 12) || (animationNo == 13) || (animationNo == 14) || (animationNo == 15);
        if (isBlocked && !getBlitzMugenAnimationRemainingAnimationTime(playerEntity))
        {
            changeBlitzMugenAnimation(playerEntity, 10);
        }
    }

    double playerSpeed = 2.f;
    void updatePlayerWalking() {
        auto animationNo = getBlitzMugenAnimationAnimationNumber(playerEntity);
        if (animationNo != 10 && animationNo != 11 && (animationNo != 12) && (animationNo != 13)) return;

        Vector2DI dir = Vector2DI(0, 0);
        if (hasPressedLeft())
        {
            dir.x += -1;
            setBlitzMugenAnimationFaceDirection(playerEntity, 0);
        }
        if (hasPressedRight())
        {
            dir.x += 1;
            setBlitzMugenAnimationFaceDirection(playerEntity, 1);
        }
        if (hasPressedUp())
        {
            dir.y += -1;
        }
        if (hasPressedDown())
        {
            dir.y += 1;
        }
        if (!dir.x && !dir.y)
        {
            if (getBlitzMugenAnimationAnimationNumber(playerEntity) == 11)
            {
                changeBlitzMugenAnimation(playerEntity, 10);
            }
            return;
        }
        changeBlitzMugenAnimationIfDifferent(playerEntity, 11);

        auto dirScaled = vecNormalize(dir) * playerSpeed;
        auto playerPosRef = getBlitzEntityPositionReference(playerEntity);
        *playerPosRef = *playerPosRef + dirScaled;
        *playerPosRef = clampPositionToGeoRectangle(*playerPosRef, GeoRectangle2D(0, playerAreaStart, 320, playerAreaEnd - playerAreaStart));
        playerPosRef->z = yToZ(playerPosRef->y);
        setBlitzMugenAnimationBaseDrawScale(playerEntity, yToScale(playerPosRef->y));
    }
    void updatePlayerPunching() {
        auto animationNo = getBlitzMugenAnimationAnimationNumber(playerEntity);
        bool isBlocked = (animationNo == 16);
        if (isBlocked) return;

        if (hasPressedAFlank())
        {
            int newAnimation = (getBlitzMugenAnimationAnimationNumber(playerEntity) == 12) ? 13 : 12;
            changeBlitzMugenAnimation(playerEntity, newAnimation);
            tryPlayMugenSoundAdvanced(&mSounds, 1, 4, sfxVol / 2.f);
        }
    }

    int maxPlayerLife = 10;
    int playerLife = 1000;
    int invincibilityFrames = 0;
    void updatePlayerGettingHit() {
        if (invincibilityFrames)
        {
            invincibilityFrames--;
            if (!invincibilityFrames)
            {
                setBlitzMugenAnimationTransparency(playerEntity, 1.0);
            }
            return;
        }

        if (hasBlitzCollidedThisFrame(playerEntity, playerPassiveCollisionId))
        {
            int animationNo = getBlitzMugenAnimationAnimationNumber(playerEntity) == 14 ? 15 : 14;
            changeBlitzMugenAnimation(playerEntity, animationNo);
            tryPlayMugenSoundAdvanced(&mSounds, 1, 1, sfxVol);
            int strength = enemyStrengths[min(gGameScreenData.mLevel, int(enemyStrengths.size() - 1))];
            playerLife = max(0, playerLife - strength);
            auto playerPos = getBlitzEntityPosition(playerEntity).xy();
            if (gGameScreenData.mSpeedLevel - gGameScreenData.mLevel < 2)
            {
                addBloodSplatter(playerPos + Vector2D(0, 10), playerPos.y + 0.001, (*getBlitzMugenAnimationBaseScaleReference(playerEntity)), !getBlitzMugenAnimationIsFacingRight(playerEntity));
            }
            invincibilityFrames = 60;
            setBlitzMugenAnimationTransparency(playerEntity, 0.7);
        }
    }
    void updatePlayerDying() {
        if (playerLife) return;

        if (getBlitzMugenAnimationAnimationNumber(playerEntity) != 16)
        {
            tryPlayMugenSoundAdvanced(&mSounds, 1, 2, sfxVol);
        }
        changeBlitzMugenAnimationIfDifferent(playerEntity, 16);
    }

    // Enemies
    struct Enemy
    {
        int entityId;
        Vector2D target;
        double speed;
        int attackCollisionId;
        int passiveCollisionId;
        int life;
        bool isToBeDeleted;
    };
    std::map<int, Enemy> mEnemies;

    void loadEnemies() {
        loadEnemySpawning();
    }
    void loadEnemySpawning() {
        int enemyCount = 10;
        for (int i = 0; i < enemyCount; i++)
        {
            addSingleEnemy();
        }
    }
    bool hasPlayedEnemyHitSoundThisFrame = false;
    int enemyPunchCooldown = 0;

    void updateEnemies() {
        if (isUpgradeScreenActive || isWaveStartActive || isWinning) return;
        updateClosestEnemy();

        hasPlayedEnemyHitSoundThisFrame = false;
        if (enemyPunchCooldown) enemyPunchCooldown--;
        auto it = mEnemies.begin();
        while (it != mEnemies.end())
        {
            if (it->second.isToBeDeleted)
            {
                unloadSingleEnemy(it->second);
                it = mEnemies.erase(it);
            }
            else
            {
                updateSingleEnemy(it->second);
                it++;
            }
        }
    }
    int closestEnemyEntity = -1;
    void updateClosestEnemy()
    {
        double closestEnemyDistance = INF;
        auto playerPos = getBlitzEntityPosition(playerEntity).xy();

        for (auto& eP : mEnemies)
        {
            auto& e = eP.second;
            auto pos = getBlitzEntityPosition(e.entityId).xy();
            auto dist = vecLength(pos - playerPos);
            if (dist < closestEnemyDistance)
            {
                closestEnemyDistance = dist;
                closestEnemyEntity = e.entityId;
            }
        }
    }

    Vector2D generateRandomPositionInPlayArea()
    {
        return Vector2D(randfrom(20, 300), randfrom(playerAreaStart, playerAreaEnd));
    }

    void addSingleEnemy() {
        Vector2D pos = generateRandomPositionInPlayArea();
        int entityId = addBlitzEntity(pos.xyz(yToZ(pos.y)));
        addBlitzMugenAnimationComponent(entityId, &mSprites, &mAnimations, 30);
        addBlitzCollisionComponent(entityId);
        auto attackCollisionId = addBlitzCollisionAttackMugen(entityId, enemyAttackCollisionList);
        auto passiveCollisionId = addBlitzCollisionPassiveMugen(entityId, enemyCollisionList);
        auto target = generateRandomPositionInPlayArea();
        double speed = 0.5f;
        int life = enemyLifes[min(gGameScreenData.mLevel, int(enemyLifes.size() - 1))];
        auto enemyPosReference = getBlitzEntityPositionReference(entityId);
        enemyPosReference->z = yToZ(enemyPosReference->y);
        setBlitzMugenAnimationBaseDrawScale(entityId, yToScale(enemyPosReference->y));

        mEnemies[entityId] = Enemy{ entityId, target, speed, attackCollisionId, passiveCollisionId, life, false };
    }
    void unloadSingleEnemy(Enemy& e) {
        removeBlitzEntity(e.entityId);
    }
    void updateSingleEnemy(Enemy& e) {
        updateSingleEnemyWalking(e);
        updateSingleEnemyAttacking(e);
        updateSingleEnemyReturningToIdle(e);
        updateSingleEnemyTurningAround(e);
        updateSingleEnemyGettingHit(e);
        updateSingleEnemyDying(e);
    }

    void updateSingleEnemyTurningAround(Enemy& e)
    {
        auto animationNo = getBlitzMugenAnimationAnimationNumber(e.entityId);
        if (animationNo != 30 && animationNo != 31) return;
        auto playerPos = getBlitzEntityPosition(playerEntity).xy();
        auto enemyPos = getBlitzEntityPosition(e.entityId).xy();
        setBlitzMugenAnimationFaceDirection(e.entityId, int(enemyPos.x < playerPos.x));
    }

    void updateSingleEnemyReturningToIdle(Enemy& e)
    {
        auto animationNo = getBlitzMugenAnimationAnimationNumber(e.entityId);
        bool isBlocked = (animationNo == 32) || (animationNo == 33) || (animationNo == 34) || (animationNo == 35);
        if (isBlocked && !getBlitzMugenAnimationRemainingAnimationTime(e.entityId))
        {
            changeBlitzMugenAnimation(e.entityId, 30);
        }
    }

    void updateSingleEnemyWalking(Enemy& e) {
        if (e.entityId == closestEnemyEntity)
        {
            updateSingleEnemyWalkingClosest(e);
        }
        else
        {
            updateSingleEnemyWalkingRegular(e);
        }
    }
    void updateSingleEnemyWalkingClosest(Enemy& e) {
        auto playerPos = getBlitzEntityPosition(playerEntity).xy();
        auto enemyPos = getBlitzEntityPosition(e.entityId).xy();

        bool isRightOfPlayer = (enemyPos.x > playerPos.x);
        auto target = Vector2D(playerPos.x + isRightOfPlayer ? 15 : -15, playerPos.y);
        updateSingleEnemyWalkingGeneral(e, target);
    }
    void updateSingleEnemyWalkingRegular(Enemy& e) {
        updateSingleEnemyWalkingGeneral(e, e.target);
    }
    void updateSingleEnemyWalkingGeneral(Enemy& e, const Vector2D& target)
    {
        auto animationNo = getBlitzMugenAnimationAnimationNumber(e.entityId);
        bool isBlocked = (animationNo == 32) || (animationNo == 33) || (animationNo == 34) || (animationNo == 35) || (animationNo == 36);
        if (isBlocked) return;

        auto enemyPosReference = getBlitzEntityPositionReference(e.entityId);

        auto dir = target - enemyPosReference->xy();
        auto dist = vecLength(dir);
        if (dist < e.speed * 2)
        {
            changeBlitzMugenAnimationIfDifferent(e.entityId, 30);
            e.target = generateRandomPositionInPlayArea();
            return;
        }

        changeBlitzMugenAnimationIfDifferent(e.entityId, 31);
        dir = vecNormalize(dir) * e.speed;
        *enemyPosReference = *enemyPosReference + dir;
        *enemyPosReference = clampPositionToGeoRectangle(*enemyPosReference, GeoRectangle2D(0, playerAreaStart, 320, playerAreaEnd - playerAreaStart));
        enemyPosReference->z = yToZ(enemyPosReference->y);
        setBlitzMugenAnimationBaseDrawScale(e.entityId, yToScale(enemyPosReference->y));
    }

    void updateSingleEnemyAttacking(Enemy& e) {
        if (e.entityId != closestEnemyEntity) return;
        if (enemyPunchCooldown) return;
        auto animationNo = getBlitzMugenAnimationAnimationNumber(e.entityId);
        bool isBlocked = (animationNo == 32) || (animationNo == 33) || (animationNo == 34) || (animationNo == 35) || (animationNo == 36);
        if (isBlocked) return;
        auto playerPos = getBlitzEntityPosition(playerEntity).xy();
        auto enemyPos = getBlitzEntityPosition(e.entityId).xy();
        if (abs(playerPos.x - enemyPos.x) < 20 && abs(playerPos.y - enemyPos.y) < 10)
        {
            int newAnimationNo = (getBlitzMugenAnimationAnimationNumber(e.entityId) == 32) ? 33 : 32;
            changeBlitzMugenAnimation(e.entityId, newAnimationNo);
            enemyPunchCooldown = 60;
        }
    }

    int bloodCounter = 0;
    void addBloodSplatter(const Vector2D& pos, double y, double scale, bool isFacingRight)
    {
        int entityId = addBlitzEntity(pos.xyz(yToZ(y)));
        addBlitzMugenAnimationComponent(entityId, &mSprites, &mAnimations, (bloodCounter % 2) ? 70 : 80);
        setBlitzMugenAnimationBaseDrawScale(entityId, scale);
        setBlitzMugenAnimationFaceDirection(entityId, isFacingRight);
        setBlitzMugenAnimationNoLoop(entityId);
        bloodCounter++;
    }

    void updateSingleEnemyGettingHit(Enemy& e) {
        if (hasBlitzCollidedThisFrame(e.entityId, e.passiveCollisionId))
        {
            int animationNo = getBlitzMugenAnimationAnimationNumber(e.entityId) == 34 ? 35 : 34;
            changeBlitzMugenAnimation(e.entityId, animationNo);
            if (!hasPlayedEnemyHitSoundThisFrame)
            {
                tryPlayMugenSoundAdvanced(&mSounds, 1, 0, sfxVol);
                hasPlayedEnemyHitSoundThisFrame = true;
            }
            auto enemyPos = getBlitzEntityPosition(e.entityId).xy();
            if (gGameScreenData.mStrengthLevel > gGameScreenData.mLevel)
            {
                addBloodSplatter(enemyPos + Vector2D(0, 10), enemyPos.y + 0.001, (*getBlitzMugenAnimationBaseScaleReference(e.entityId)), !getBlitzMugenAnimationIsFacingRight(e.entityId));
            }
            e.life = max(0, e.life - playerStrength);
        }
    }
    void updateSingleEnemyDying(Enemy& e) {
        if (!e.life)
        {
            if (getBlitzMugenAnimationAnimationNumber(e.entityId) != 36)
            {
                tryPlayMugenSoundAdvanced(&mSounds, 1, 3, sfxVol);
                auto enemyPos = getBlitzEntityPosition(e.entityId).xy();
                int loveGain = loveGains[min(gGameScreenData.mLevel, 2)];
                gGameScreenData.mPlayerLoveCount += loveGain;
                addPrismNumberPopup(loveGain, enemyPos.xyz(30) - Vector2D(0, 30 * (*getBlitzMugenAnimationBaseScaleReference(e.entityId))), 1, Vector3D(0, -1.f * (*getBlitzMugenAnimationBaseScaleReference(e.entityId)), 0), *getBlitzMugenAnimationBaseScaleReference(e.entityId), 0, 20);
            }
            changeBlitzMugenAnimationIfDifferent(e.entityId, 36);

            if (getBlitzMugenAnimationAnimationStep(e.entityId) == 5)
            {
                e.isToBeDeleted = true;
            }
        }
    }

    // UI
    MugenAnimationHandlerElement* lifebarBG;
    MugenAnimationHandlerElement* lifebarFG;
    int loveCounterTextId;
    int loveCounterBackgroundTextId;
    MugenAnimationHandlerElement* loveCounter;
    void loadUI() {
        lifebarBG = addMugenAnimation(getMugenAnimation(&mAnimations, 50), &mSprites, Vector3D(70, 230, 30));
        lifebarFG = addMugenAnimation(getMugenAnimation(&mAnimations, 51), &mSprites, Vector3D(70, 230, 30));
        loveCounterTextId = addMugenTextMugenStyle("0", Vector3D(90, 218, 31), Vector3DI(1, 0, 1));
        setMugenTextColorRGB(loveCounterTextId, 232 / 256.0, 106 / 256.0, 115 / 256.0);
        setMugenTextScale(loveCounterTextId, 2.0);
        loveCounterBackgroundTextId = addMugenTextMugenStyle("0", Vector3D(91, 219, 30), Vector3DI(1, 0, 1));
        setMugenTextColorRGB(loveCounterBackgroundTextId, 32 / 256.0, 214 / 256.0, 199 / 256.0);
        setMugenTextScale(loveCounterBackgroundTextId, 2.0);
        loveCounter = addMugenAnimation(getMugenAnimation(&mAnimations, 60), &mSprites, Vector3D(23, 210, 30));
        setMugenAnimationVisibility(lifebarBG, 0);
        setMugenAnimationVisibility(lifebarFG, 0);
        setMugenAnimationVisibility(loveCounter, 0);
        setMugenTextVisibility(loveCounterTextId, 0);
        setMugenTextVisibility(loveCounterBackgroundTextId, 0);
        updateUI();
    }
    void updateUI() {
        if (isUpgradeScreenActive) return;
        updateLifebar();
        updateLoveCounter();
    }
    void updateLifebar() {
        double t = playerLife / double(maxPlayerLife);
        setMugenAnimationRectangleWidth(lifebarFG, int(216 * t));
    }
    void updateLoveCounter() {
        string t = std::to_string(gGameScreenData.mPlayerLoveCount);
        changeMugenText(loveCounterTextId, t.c_str());
        changeMugenText(loveCounterBackgroundTextId, t.c_str());
    }

    // Winning
    MugenAnimationHandlerElement* winningAnimation;
    bool isWinning = false;
    int winningTicks = 0;
    void loadWinning() {
        winningAnimation = addMugenAnimation(getMugenAnimation(&mAnimations, 100), &mSprites, Vector3D(0, 0, 40));
        setMugenAnimationVisibility(winningAnimation, false);
    }
    void updateWinning() {
        if (isUpgradeScreenActive) return;
        updateWinningStart();
        updateWinningActive();
    }
    void updateWinningStart() {
        if (isWinning) return;

        if (mEnemies.empty())
        {
            setMugenAnimationVisibility(winningAnimation, true);
            isWinning = true;
            pauseBlitzMugenAnimation(playerEntity);
            setMugenAnimationVisibility(lifebarBG, 0);
            setMugenAnimationVisibility(lifebarFG, 0);
            setMugenAnimationVisibility(loveCounter, 0);
            setMugenTextVisibility(loveCounterTextId, 0);
            setMugenTextVisibility(loveCounterBackgroundTextId, 0);
            stopStreamingMusicFile();
            tryPlayMugenSoundAdvanced(&mSounds, 100, 0, 1.0);
        }
    }
    void updateWinningActive() {
        if (!isWinning) return;

        winningTicks++;
        if (hasPressedStartFlank() || winningTicks > 600)
        {
            gGameScreenData.mLevel++;
            if (gGameScreenData.mLevel == 3)
            {
                setBookName("outro");
                setNewScreen(getBookScreen());
            }
            else
            {
                setNewScreen(getGameScreen());
            }
        }
    }

    // Upgrade screen
    MugenAnimationHandlerElement* upgradeBG;
    MugenAnimationHandlerElement* upgradeBG2;
    MugenAnimationHandlerElement* upgradeBuyPointer;

    std::vector<int> levelCosts = { 0, 50, 5000, 500000, 10 * 10 * 10 * 10 * 10 * 10 };

    int loveCostStrengthTextId;
    int loveCostSpeedTextId;
    int selectedUpgradeIndex = 0;
    void loadUpgradeScreen() {
        upgradeBG = addMugenAnimation(getMugenAnimation(&mAnimations, 120), &mSprites, Vector3D(0, 0, 40));
        setMugenAnimationVisibility(upgradeBG, false);
        upgradeBG2 = addMugenAnimation(getMugenAnimation(&mAnimations, 130), &mSprites, Vector3D(0, 0, 41));
        setMugenAnimationVisibility(upgradeBG2, false);
        upgradeBuyPointer = addMugenAnimation(getMugenAnimation(&mAnimations, 140), &mSprites, Vector3D(40, 100, 42));
        setMugenAnimationVisibility(upgradeBuyPointer, false);
        loveCostStrengthTextId = addMugenTextMugenStyle("0", Vector3D(170, 118, 42), Vector3DI(2, 0, 1));
        setMugenTextColorRGB(loveCostStrengthTextId, 232 / 256.0, 106 / 256.0, 115 / 256.0);
        setMugenTextVisibility(loveCostStrengthTextId, false);
        setMugenTextScale(loveCostStrengthTextId, 2.0);

        loveCostSpeedTextId = addMugenTextMugenStyle("0", Vector3D(170, 118 + 37, 42), Vector3DI(2, 0, 1));
        setMugenTextColorRGB(loveCostSpeedTextId, 232 / 256.0, 106 / 256.0, 115 / 256.0);
        setMugenTextVisibility(loveCostSpeedTextId, false);
        setMugenTextScale(loveCostSpeedTextId, 2.0);
    }
    bool isUpgradeScreenActive = false;
    void updateUpgradeScreen() {
        updateUpgradeScreenStart();
        updateUpgradeScreenActive();
    }
    bool isUpgradeScreenGameOver = false;
    void updateUpgradeScreenStart() {
        if (isUpgradeScreenActive) return;

        if (!playerLife && getBlitzMugenAnimationAnimationNumber(playerEntity) == 16 && getBlitzMugenAnimationAnimationStep(playerEntity) == 5)
        {
            
            stopStreamingMusicFile();
            setMugenAnimationVisibility(upgradeBG, true);
            setMugenAnimationVisibility(upgradeBG2, true);
            if (levelCosts[gGameScreenData.mStrengthLevel] > gGameScreenData.mPlayerLoveCount && levelCosts[gGameScreenData.mSpeedLevel] > gGameScreenData.mPlayerLoveCount)
            {
                changeMugenAnimation(upgradeBG2, getMugenAnimation(&mAnimations, 110));
                isUpgradeScreenGameOver = true;
                tryPlayMugenSoundAdvanced(&mSounds, 100, 1, 1.0);
            }
            else
            {
                setMugenAnimationVisibility(upgradeBuyPointer, true);
                setMugenTextVisibility(loveCostStrengthTextId, true);
                setMugenTextVisibility(loveCostSpeedTextId, true);
                changeMugenText(loveCostStrengthTextId, std::to_string(levelCosts[gGameScreenData.mStrengthLevel]).c_str());
                changeMugenText(loveCostSpeedTextId, std::to_string(levelCosts[gGameScreenData.mSpeedLevel]).c_str());

                double xOffset = 20;
                double yOffset = -30;
                auto loveCounterPosReference = getMugenAnimationPositionReference(loveCounter);
                loveCounterPosReference->z = 45;
                loveCounterPosReference->x += xOffset;
                loveCounterPosReference->y += yOffset;
                auto loveCounterTextPosReference = getMugenTextPositionReference(loveCounterTextId);
                loveCounterTextPosReference->z = 45;
                loveCounterTextPosReference->x += xOffset;
                loveCounterTextPosReference->y += yOffset;
                auto loveCounterBackgroundTextPosReference = getMugenTextPositionReference(loveCounterBackgroundTextId);
                loveCounterBackgroundTextPosReference->z = 45;
                loveCounterBackgroundTextPosReference->x += xOffset;
                loveCounterBackgroundTextPosReference->y += yOffset;

                updateUpgradeScreenUI();
            }
            isUpgradeScreenActive = true;
        }
    }
    void updateUpgradeScreenActive() {
        if (!isUpgradeScreenActive) return;
        gGameScreenData.mGameTicks++;
        if (!isUpgradeScreenGameOver)
        {
            updateUpgradeScreenMoveSelection();
            updateUpgradeScreenConfirmSelection();
            updateUpgradeScreenUI();
        }
        else
        {
            updateUpgradeScreenGameOver();
        }
    }
    void updateUpgradeScreenGameOver() {
        if (hasPressedStartFlank())
        {
            resetGame();
            setBookName("intro");
            setNewScreen(getBookScreen());
        }
    }

    void updateUpgradeScreenMoveSelection() {
        if (hasPressedUpFlank() || hasPressedDownFlank())
        {
            tryPlayMugenSoundAdvanced(&mSounds, 2, 0, sfxVol);
            selectedUpgradeIndex = (selectedUpgradeIndex + 1) % 2;
        }
    }
    void updateUpgradeScreenConfirmSelection() {
        if (hasPressedAFlank())
        {
            int currentLevel = selectedUpgradeIndex ? gGameScreenData.mSpeedLevel : gGameScreenData.mStrengthLevel;
            int necessaryLove = currentLevel >= 3 ? 1000000000 : levelCosts[currentLevel];
            if (necessaryLove > gGameScreenData.mPlayerLoveCount)
            {
                tryPlayMugenSoundAdvanced(&mSounds, 2, 2, sfxVol);
            }
            else
            {
                tryPlayMugenSoundAdvanced(&mSounds, 2, 1, sfxVol);
                gGameScreenData.mPlayerLoveCount -= necessaryLove;
                if (selectedUpgradeIndex)
                {
                    gGameScreenData.mSpeedLevel++;
                }
                else
                {
                    gGameScreenData.mStrengthLevel++;
                }
                setNewScreen(getGameScreen());
            }
        }
    }

    void updateUpgradeScreenUI() {
        auto pointerPos = getMugenAnimationPositionReference(upgradeBuyPointer);
        pointerPos->y = 100 + 37 * selectedUpgradeIndex;
    }
};

EXPORT_SCREEN_CLASS(GameScreen);

void resetGame()
{
    gGameScreenData.mLevel = 0;
    gGameScreenData.mPlayerLoveCount = 0;
    gGameScreenData.mStrengthLevel = 0;
    gGameScreenData.mSpeedLevel = 0;
    gGameScreenData.mGameTicks = 0;
}

std::string getSpeedRunString() {
    int totalSeconds = gGameScreenData.mGameTicks / 60;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    int milliseconds = (gGameScreenData.mGameTicks % 60) * 1000 / 60;
    return std::to_string(minutes) + "m " + std::to_string(seconds) + "s " + std::to_string(milliseconds) + "ms.";
}