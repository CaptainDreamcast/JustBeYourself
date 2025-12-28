#include "bookscreen.h"

#include <prism/blitz.h>
#include <prism/soundeffect.h>

#include "gamescreen.h"
struct
{
	std::string mBookName = "intro";

} gBookScreenData;

static bool isGameInitialized = false;

void gotoVNScreenCB(void*);

class BookScreen {
public:

	struct BookTextPart
	{
		std::string text;
	};

	struct BookText
	{
		std::vector<BookTextPart> mParts;
	};

	std::map<std::string, BookText> mTexts;
	BookScreen()
	{
		loadBookTexts();
		loadFiles();
		loadScreenEntities();
		setTextActive();
		resetGame();
		streamMusicFile("game/STORY.ogg");
	}

	void loadBookTexts()
	{
		MugenDefScript script;
		loadMugenDefScript(&script, "game/STORY.def");
		MugenDefScriptGroup* group = script.mFirstGroup;
		while (group)
		{
			loadBookTextFromSingleGroup(group);
			group = group->mNext;
		}

		unloadMugenDefScript(&script);
	}

	void loadBookTextFromSingleGroup(MugenDefScriptGroup* group)
	{
		auto& conversation = mTexts[group->mName];
		auto iterator = list_iterator_begin(&group->mOrderedElementList);
		while (iterator)
		{
			BookTextPart part;
			MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)list_iterator_get(iterator);
			part.text = getSTLMugenDefStringVariableAsElementForceAddWhiteSpaces(element);
			conversation.mParts.push_back(part);

			if (!list_has_next(iterator))
			{
				iterator = nullptr;
			}
			else {
				list_iterator_increase(&iterator);
			}
		}
	}

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;
	MugenSounds mSoundsGeneral;
	BookText* mActiveBookText;

	void loadFiles()
	{
		turnStringUppercase(gBookScreenData.mBookName);
		mSprites = loadMugenSpriteFileWithoutPalette(std::string("game/") + gBookScreenData.mBookName +".sff");
		mAnimations = loadMugenAnimationFile(std::string("game/") + gBookScreenData.mBookName + ".air");
		mSounds = loadMugenSoundFile((std::string("game/") + gBookScreenData.mBookName + ".snd").c_str());
		mSoundsGeneral = loadMugenSoundFile("game/BOOK.snd");

		turnStringLowercase(gBookScreenData.mBookName);
		assert(mTexts.find(gBookScreenData.mBookName) != mTexts.end());
		mActiveBookText = &mTexts[gBookScreenData.mBookName];
	}

	int mLeftAnimationBG;
	int mLeftAnimationFG;
	int mRightAnimationBG;
	int mRightAnimationFG;
	int mTextId;
	int mRightSelected = 0;
	void loadScreenEntities()
	{
		mLeftAnimationBG = addBlitzEntity(Vector3D(160, 0, 1));
		addBlitzMugenAnimationComponent(mLeftAnimationBG, &mSprites, &mAnimations, -1);

		mLeftAnimationFG = addBlitzEntity(Vector3D(160, 0, 2));
		addBlitzMugenAnimationComponent(mLeftAnimationFG, &mSprites, &mAnimations, -1);

		mRightAnimationBG = addBlitzEntity(Vector3D(160, 0, 1));
		addBlitzMugenAnimationComponent(mRightAnimationBG, &mSprites, &mAnimations, -1);

		mRightAnimationFG = addBlitzEntity(Vector3D(160, 0, 2));
		addBlitzMugenAnimationComponent(mRightAnimationFG, &mSprites, &mAnimations, -1);

		mTextId = addMugenTextMugenStyle(" ", Vector3D(40, 200, 3), Vector3DI(1, 7, 1));
		setMugenTextScale(mTextId, 1.0);
		setMugenTextTextBoxWidth(mTextId, 240);
		loadInitialAnimations();
	}

	int currentLoadedVoiceSoundEffectId = -1;
	int currentPlayingVoiceSoundEffectId = -1;
	void playVoiceClip()
	{
		if (isOnDreamcast()) return;
		stopAllSoundEffects();
		tryPlayMugenSound(&mSounds, 1, mRightSelected);
	}

	void  loadInitialAnimations()
	{
		changeBlitzMugenAnimation(mLeftAnimationBG, 1000);
		changeBlitzMugenAnimation(mRightAnimationBG, 1001);
		mRightSelected = 0;
	}

	int isFadingOut = 0;
	void update()
	{
		if (isFadingOut) return;
		updateScreenInput();
		updateFlipping();
	}

	int isFlippingPage = 0;
	void updateScreenInput() {
		if (!isGameInitialized && hasPressedMouseLeftFlank())
		{
			stopAllSoundEffects();
			tryPlayMugenSound(&mSounds, 1, mRightSelected);
			isGameInitialized = true;
		}
		if (isFlippingPage)
		{
			if (hasPressedLeftFlank() || hasPressedRightFlank() || hasPressedAFlank() || hasPressedStartFlank())
			{
				finishFlipping();
			}
		}
		else
		{
			if (hasPressedRightFlank() || hasPressedAFlank() || hasPressedStartFlank())
			{
				if (getMugenTextVisibility(mTextId) && !isMugenTextBuiltUp(mTextId))
				{
					setMugenTextBuiltUp(mTextId);
				}
				else
				{
					flipPageRight();
				}
			}
			else if (hasPressedLeftFlank())
			{
				//flipPageLeft();
			}
		}
	}

	float EaseIn(float t)
	{
		return t * t;
	}
	float Flip(float x)
	{
		return 1 - x;
	}
	float Square(float x)
	{
		return x * x;
	}
	float EaseOut(float t)
	{
		return Flip(Square(Flip(t)));
	}

	double flipT = 0;
	int flippingStage = 0;
	void flipPageRight()
	{
		if (isFinalPage())
		{
			gotoVNScreen();
			return;
		}


		if (currentPlayingVoiceSoundEffectId != -1)
		{
			stopSoundEffect(currentPlayingVoiceSoundEffectId);
			currentPlayingVoiceSoundEffectId = -1;
		}
		if (currentLoadedVoiceSoundEffectId != -1)
		{

			unloadSoundEffect(currentLoadedVoiceSoundEffectId);
			currentLoadedVoiceSoundEffectId = -1;
		}
		tryPlayMugenSound(&mSoundsGeneral, 1, 0);


		mRightSelected++;
		changeBlitzMugenAnimation(mRightAnimationFG, getBlitzMugenAnimationAnimationNumber(mRightAnimationBG));
		changeBlitzMugenAnimation(mRightAnimationBG, 1000 + mRightSelected * 2 + 1);
		setMugenTextVisibility(mTextId, false);

		isFlippingPage = 1;
		flipT = 0;
		flippingStage = 0;
	}

	void updateFlipping()
	{
		if (isFlippingPage == 1)
		{
			updateFlippingRight();
		}
		else if (isFlippingPage == 2)
		{
			//updateFlippingLeft();
		}
	}

	void updateFlippingRight()
	{
		if (flippingStage == 0)
		{
			updateFlippingRight1();
		}
		else {
			updateFlippingRight2();
		}
	}

	void updateFlippingRight1()
	{
		auto scale = getBlitzMugenAnimationDrawScale(mRightAnimationFG);
		scale.x = Flip(EaseIn(flipT));
		flipT += 0.05;
		if (scale.x < 0.01)
		{
			scale.x = 0;
		}
		setBlitzMugenAnimationDrawScale(mRightAnimationFG, scale);
		if (scale.x == 0)
		{
			finishFlippingRight1();
		}
	}




	void finishFlippingRight1()
	{
		changeBlitzMugenAnimation(mRightAnimationFG, -1);
		setBlitzMugenAnimationDrawScale(mRightAnimationFG, Vector2D(1, 1));
		startFlippingRight2();
	}

	void startFlippingRight2()
	{
		changeBlitzMugenAnimation(mLeftAnimationFG, 1000 + mRightSelected * 2);
		setBlitzMugenAnimationDrawScale(mLeftAnimationFG, Vector2D(0, 1));
		flippingStage = 1;
		flipT = 0;
	}

	void finishFlipping()
	{
		if (flippingStage == 0) finishFlippingRight1();
		if (flippingStage == 1) finishFlippingRight2();
	}

	void updateFlippingRight2()
	{
		auto scale = getBlitzMugenAnimationDrawScale(mLeftAnimationFG);
		scale.x = EaseOut(flipT);
		flipT += 0.05;
		if (scale.x > 0.99)
		{
			scale.x = 1.0;
		}
		setBlitzMugenAnimationDrawScale(mLeftAnimationFG, scale);
		if (scale.x == 1.0)
		{
			finishFlippingRight2();
		}
	}

	void finishFlippingRight2()
	{
		changeBlitzMugenAnimation(mLeftAnimationBG, getBlitzMugenAnimationAnimationNumber(mLeftAnimationFG));
		changeBlitzMugenAnimation(mLeftAnimationFG, -1);
		setBlitzMugenAnimationDrawScale(mLeftAnimationFG, Vector2D(0, 1));
		isFlippingPage = 0;

		setTextActive();
	}

	void setTextActive()
	{
		auto& bookPart = mActiveBookText->mParts[mRightSelected];
		playVoiceClip();
		if (bookPart.text == "end" || bookPart.text == "title") return;
		if (gBookScreenData.mBookName == "outro" && mRightSelected == 3)
		{
			bookPart.text += getSpeedRunString();
		}
		changeMugenText(mTextId, bookPart.text.c_str());
		setMugenTextBuildup(mTextId, 1);
		setMugenTextVisibility(mTextId, true);
	}

	int isFinalPage()
	{
		return mRightSelected == mActiveBookText->mParts.size() - 1;
	}

	void gotoVNScreen()
	{
		addFadeOut(20, gotoVNScreenCB);
		isFadingOut = 1;
	}

};

EXPORT_SCREEN_CLASS(BookScreen);

void gotoVNScreenCB(void*)
{
	if (gBookScreenData.mBookName == "outro")
	{
		setBookName("intro");
		setNewScreen(getBookScreen());
	}
	else
	{
		setNewScreen(getGameScreen());
	}
}

void setBookName(const std::string& name)
{
	gBookScreenData.mBookName = name;
}