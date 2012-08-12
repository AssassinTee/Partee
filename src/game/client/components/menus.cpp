/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <math.h>

#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <game/version.h>
#include <game/generated/protocol.h>

#include <game/generated/client_data.h>
#include <game/client/components/camera.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <mastersrv/mastersrv.h>

#include "maplayers.h"
#include "countryflags.h"
#include "menus.h"
#include "skins.h"

float CMenus::ms_ButtonHeight = 25.0f;
float CMenus::ms_ListheaderHeight = 17.0f;
float CMenus::ms_FontmodHeight = 0.8f;

IInput::CEvent CMenus::m_aInputEvents[MAX_INPUTEVENTS];
int CMenus::m_NumInputEvents;


CMenus::CMenus()
{
	m_Popup = POPUP_NONE;
	m_ActivePage = PAGE_INTERNET;
	m_GamePage = PAGE_GAME;

	m_NeedRestartGraphics = false;
	m_NeedRestartSound = false;
	m_TeePartSelection = NO_SELECTION;
	m_TeePartsColorSelection = NO_SELECTION;
	m_aSaveSkinName[0] = 0;
	m_MenuActive = true;
	m_UseMouseButtons = true;

	m_MenuPage = PAGE_START;

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;

	m_LastInput = time_get();

	str_copy(m_aCurrentDemoFolder, "demos", sizeof(m_aCurrentDemoFolder));
	m_aCallvoteReason[0] = 0;

	m_FriendlistSelectedIndex = -1;

	m_SelectedFilter = 0;

	m_SelectedServer.m_Filter = -1;
	m_SelectedServer.m_Index = -1;
}

float *CMenus::ButtonFade(const void *pID, float Seconds, int Checked)
{
	float *pFade = (float*)pID;
	if(UI()->HotItem() == pID || Checked)
		*pFade = Seconds;
	else if(*pFade > 0.0f)
	{
		*pFade -= Client()->RenderFrameTime();
		if(*pFade < 0.0f)
			*pFade = 0.0f;
	}
	return pFade;
}

int CMenus::DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect)
{
	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);

	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SpriteId);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return 0;
}

int CMenus::DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUIBUTTONS].m_Id);
	Graphics()->QuadsBegin();
	if(!Active)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(Checked?SPRITE_GUIBUTTON_ON:SPRITE_GUIBUTTON_OFF);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	if(UI()->HotItem() == pID && Active)
	{
		RenderTools()->SelectSprite(SPRITE_GUIBUTTON_HOVER);
		IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
	Graphics()->QuadsEnd();

	return Active ? UI()->DoButtonLogic(pID, "", Checked, pRect) : 0;
}

int CMenus::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, float r, float FontFactor, int Corners)
{
	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	RenderTools()->DrawUIRect(pRect, vec4(1.0f , 1.0f, 1.0f, 0.5f+(*pFade/Seconds)*0.25f), Corners, r);
	CUIRect Temp;
	pRect->HMargin(pRect->h>=20.0f?2.0f:1.0f, &Temp);
	Temp.HMargin((Temp.h*FontFactor)/2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_MenuImage(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pImageName, float r, float FontFactor)
{
	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	RenderTools()->DrawUIRect(pRect, vec4(1.0f , 1.0f, 1.0f, 0.5f+(*pFade/Seconds)*0.25f), CUI::CORNER_ALL, r);
	CUIRect Text, Image;
	pRect->VSplitRight(pRect->h*4.0f, &Text, &Image); // always correct ratio for image

	// render image
	const CMenuImage *pImage = FindMenuImage(pImageName);
	if(pImage)
	{
		
		Graphics()->TextureSet(pImage->m_GreyTexture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		IGraphics::CQuadItem QuadItem(Image.x, Image.y, Image.w, Image.h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();

		if(*pFade > 0.0f)
		{
			Graphics()->TextureSet(pImage->m_OrgTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, *pFade/Seconds);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
		
	}

	pRect->HMargin(pRect->h>=20.0f?2.0f:1.0f, &Text);
	Text.HMargin((Text.h*FontFactor)/2.0f, &Text);
	Text.VSplitLeft(r, 0, &Text);
	UI()->DoLabel(&Text, pText, Text.h*ms_FontmodHeight, -1);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

void CMenus::DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	RenderTools()->DrawUIRect(pRect, vec4(1.0f , 1.0f, 1.0f, 0.5f+(*pFade/Seconds)*0.25f), CUI::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);
}

int CMenus::DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	if(Checked)
		RenderTools()->DrawUIRect(pRect, vec4(1.0f, 1.0f, 1.0f, 0.25f), Corners, 10.0f);
	else
	{
		RenderTools()->DrawUIRect(pRect, vec4(0.0f, 0.0f, 0.0f, 0.25f), Corners, 10.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.7f);
	}

	CUIRect Temp;
	pRect->HMargin(2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_MenuTabTop(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds, Checked);

	RenderTools()->DrawUIRect(pRect, vec4(1.0f , 1.0f, 1.0f, 0.5f+(*pFade/Seconds)*0.25f), Corners, 12.0f);

	CUIRect Temp;
	pRect->HMargin(2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h*ms_FontmodHeight, 0);

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
//void CMenus::ui_draw_grid_header(const void *id, const char *text, int checked, const CUIRect *r, const void *extra)
{
	if(Checked)
		RenderTools()->DrawUIRect(pRect, vec4(1,1,1,0.5f), 0, 0.0f);
	CUIRect t;
	pRect->VSplitLeft(5.0f, 0, &t);
	UI()->DoLabel(&t, pText, pRect->h*ms_FontmodHeight, -1);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_GridHeaderIcon(const void *pID, int ImageID, int SpriteID, const CUIRect *pRect, int Corners)
{
	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	RenderTools()->DrawUIRect(pRect, vec4(1.0f, 1.0f, 1.0f, 0.5f+(*pFade/Seconds)*0.25f), Corners, 5.0f);

	CUIRect Image;
	pRect->HMargin(2.0f, &Image);
	pRect->VMargin((Image.w-Image.h)/2.0f, &Image);

	Graphics()->TextureSet(g_pData->m_aImages[ImageID].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(SpriteID);
	IGraphics::CQuadItem QuadItem(Image.x, Image.y, Image.w, Image.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return UI()->DoButtonLogic(pID, "", false, pRect);
}

int CMenus::DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect)
//void CMenus::ui_draw_checkbox_common(const void *id, const char *text, const char *boxtext, const CUIRect *r, const void *extra)
{
	CUIRect c = *pRect;
	CUIRect t = *pRect;
	c.w = c.h;
	t.x += c.w;
	t.w -= c.w;
	t.VSplitLeft(5.0f, 0, &t);
	
	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	c.Margin(2.0f, &c);
	RenderTools()->DrawUIRect(&c, vec4(1.0f , 1.0f, 1.0f, 0.25f+(*pFade/Seconds)*0.125f), CUI::CORNER_ALL, 3.0f);
	c.y += 2;
	UI()->DoLabel(&c, pBoxText, pRect->h*ms_FontmodHeight*0.6f, 0);
	UI()->DoLabel(&t, pText, pRect->h*ms_FontmodHeight*0.8f, -1);
	return UI()->DoButtonLogic(pID, pText, 0, pRect);
}

int CMenus::DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoButton_CheckBox_Common(pID, pText, Checked?"X":"", pRect);
}


int CMenus::DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Checked);
	return DoButton_CheckBox_Common(pID, pText, aBuf, pRect);
}

int CMenus::DoButton_SpriteClean(int ImageID, int SpriteID, const CUIRect *pRect)
{
	int Inside = UI()->MouseInside(pRect);

	Graphics()->TextureSet(g_pData->m_aImages[ImageID].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Inside ? 1.0f : 0.6f);
	RenderTools()->SelectSprite(SpriteID);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	int ReturnValue = 0;
	if(Inside && Input()->KeyDown(KEY_MOUSE_1))
		ReturnValue = 1;

	return ReturnValue;
}

int CMenus::DoButton_SpriteCleanID(const void *pID, int ImageID, int SpriteID, const CUIRect *pRect)
{
	int Inside = UI()->MouseInside(pRect);

	Graphics()->TextureSet(g_pData->m_aImages[ImageID].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Inside ? 1.0f : 0.6f);
	RenderTools()->SelectSprite(SpriteID);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return UI()->DoButtonLogic(pID, 0, 0, pRect);
}

int CMenus::DoButton_MouseOver(int ImageID, int SpriteID, const CUIRect *pRect)
{
	int Inside = UI()->MouseInside(pRect);

	Graphics()->TextureSet(g_pData->m_aImages[ImageID].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Inside ? 1.0f : 0.6f);
	RenderTools()->SelectSprite(SpriteID);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return Inside;
}

int CMenus::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners)
{
	int Inside = UI()->MouseInside(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;
	static int s_AtIndex = 0;
	static bool s_DoScroll = false;
	static float s_ScrollStart = 0.0f;

	FontSize *= UI()->Scale();

	if(UI()->LastActiveItem() == pID)
	{
		int Len = str_length(pStr);
		if(Len == 0)
			s_AtIndex = 0;

		if(Inside && UI()->MouseButton(0))
		{
			s_DoScroll = true;
			s_ScrollStart = UI()->MouseX();
			int MxRel = (int)(UI()->MouseX() - pRect->x);

			for(int i = 1; i <= Len; i++)
			{
				if(TextRender()->TextWidth(0, FontSize, pStr, i) - *Offset > MxRel)
				{
					s_AtIndex = i - 1;
					break;
				}

				if(i == Len)
					s_AtIndex = Len;
			}
		}
		else if(!UI()->MouseButton(0))
			s_DoScroll = false;
		else if(s_DoScroll)
		{
			// do scrolling
			if(UI()->MouseX() < pRect->x && s_ScrollStart-UI()->MouseX() > 10.0f)
			{
				s_AtIndex = max(0, s_AtIndex-1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
			else if(UI()->MouseX() > pRect->x+pRect->w && UI()->MouseX()-s_ScrollStart > 10.0f)
			{
				s_AtIndex = min(Len, s_AtIndex+1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
		}

		for(int i = 0; i < m_NumInputEvents; i++)
		{
			Len = str_length(pStr);
			ReturnValue |= CLineInput::Manipulate(m_aInputEvents[i], pStr, StrSize, &Len, &s_AtIndex);
		}
	}

	bool JustGotActive = false;

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			s_AtIndex = min(s_AtIndex, str_length(pStr));
			s_DoScroll = false;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			if (UI()->LastActiveItem() != pID)
				JustGotActive = true;
			UI()->SetActiveItem(pID);
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	CUIRect Textbox = *pRect;
	RenderTools()->DrawUIRect(&Textbox, vec4(1, 1, 1, 0.5f), Corners, 3.0f);
	Textbox.VMargin(2.0f, &Textbox);
	Textbox.HMargin(2.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(Hidden)
	{
		unsigned s = str_length(pStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars)-1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	// check if the text has to be moved
	if(UI()->LastActiveItem() == pID && !JustGotActive && (UpdateOffset || m_NumInputEvents))
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		if(w-*Offset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1);
			do
			{
				*Offset += min(wt-*Offset-Textbox.w, Textbox.w/3);
			}
			while(w-*Offset > Textbox.w);
		}
		else if(w-*Offset < 0.0f)
		{
			// move to the right
			do
			{
				*Offset = max(0.0f, *Offset-Textbox.w/3);
			}
			while(w-*Offset < 0.0f);
		}
	}
	UI()->ClipEnable(pRect);
	Textbox.x -= *Offset;

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1);

	// render the cursor
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, s_AtIndex);
		Textbox = *pRect;
		Textbox.VSplitLeft(2.0f, 0, &Textbox);
		Textbox.x += (w-*Offset-TextRender()->TextWidth(0, FontSize, "|", -1)/2);

		if((2*time_get()/time_freq()) % 2)	// make it blink
			UI()->DoLabel(&Textbox, "|", FontSize, -1);
	}
	UI()->ClipDisable();

	return ReturnValue;
}

float CMenus::DoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetY;
	pRect->HSplitTop(33, &Handle, 0);

	Handle.y += (pRect->h-Handle.h)*Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->y;
		float Max = pRect->h-Handle.h;
		float Cur = UI()->MouseY()-OffsetY;
		ReturnValue = (Cur-Min)/Max;
		if(ReturnValue < 0.0f) ReturnValue = 0.0f;
		if(ReturnValue > 1.0f) ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetY = UI()->MouseY()-Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	CUIRect Rail;
	pRect->VMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, vec4(1,1,1,0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.w = Rail.x-Slider.x;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_L, 2.5f);
	Slider.x = Rail.x+Rail.w;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_R, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);

	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	RenderTools()->DrawUIRect(&Slider, vec4(1.0f , 1.0f, 1.0f, 0.25f+(*pFade/Seconds)*0.125f), CUI::CORNER_ALL, 2.5f);

	return ReturnValue;
}



float CMenus::DoScrollbarH(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetX;
	pRect->VSplitLeft(33, &Handle, 0);

	Handle.x += (pRect->w-Handle.w)*Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->x;
		float Max = pRect->w-Handle.w;
		float Cur = UI()->MouseX()-OffsetX;
		ReturnValue = (Cur-Min)/Max;
		if(ReturnValue < 0.0f) ReturnValue = 0.0f;
		if(ReturnValue > 1.0f) ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetX = UI()->MouseX()-Handle.x;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	CUIRect Rail;
	pRect->HMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, vec4(1,1,1,0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.h = Rail.y-Slider.y;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_T, 2.5f);
	Slider.y = Rail.y+Rail.h;
	RenderTools()->DrawUIRect(&Slider, vec4(1,1,1,0.25f), CUI::CORNER_B, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);

	float Seconds = 0.6f; //  0.6 seconds for fade
	float *pFade = ButtonFade(pID, Seconds);

	RenderTools()->DrawUIRect(&Slider, vec4(1.0f , 1.0f, 1.0f, 0.25f+(*pFade/Seconds)*0.125f), CUI::CORNER_ALL, 2.5f);

	return ReturnValue;
}

int CMenus::DoKeyReader(void *pID, const CUIRect *pRect, int Key)
{
	// process
	static void *pGrabbedID = 0;
	static bool MouseReleased = true;
	static int ButtonUsed = 0;
	int Inside = UI()->MouseInside(pRect);
	int NewKey = Key;

	if(!UI()->MouseButton(0) && !UI()->MouseButton(1) && pGrabbedID == pID)
		MouseReleased = true;

	if(UI()->ActiveItem() == pID)
	{
		if(m_Binder.m_GotKey)
		{
			// abort with escape key
			if(m_Binder.m_Key.m_Key != KEY_ESCAPE)
				NewKey = m_Binder.m_Key.m_Key;
			m_Binder.m_GotKey = false;
			UI()->SetActiveItem(0);
			MouseReleased = false;
			pGrabbedID = pID;
		}

		if(ButtonUsed == 1 && !UI()->MouseButton(1))
		{
			if(Inside)
				NewKey = 0;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(MouseReleased)
		{
			if(UI()->MouseButton(0))
			{
				m_Binder.m_TakeKey = true;
				m_Binder.m_GotKey = false;
				UI()->SetActiveItem(pID);
				ButtonUsed = 0;
			}

			if(UI()->MouseButton(1))
			{
				UI()->SetActiveItem(pID);
				ButtonUsed = 1;
			}
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// draw
	if (UI()->ActiveItem() == pID && ButtonUsed == 0)
		DoButton_KeySelect(pID, "???", 0, pRect);
	else
	{
		if(Key == 0)
			DoButton_KeySelect(pID, "", 0, pRect);
		else
			DoButton_KeySelect(pID, Input()->KeyName(Key), 0, pRect);
	}
	return NewKey;
}


int CMenus::RenderMenubar(CUIRect r)
{
	CUIRect Box = r;
	CUIRect Button;

	m_ActivePage = m_MenuPage;
	int NewPage = -1;

	if(Client()->State() != IClient::STATE_OFFLINE)
		m_ActivePage = m_GamePage;

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		/*/ offline menus
		if(0) // this is not done yet
		{
			Box.VSplitLeft(90.0f, &Button, &Box);
			static int s_NewsButton=0;
			if (DoButton_MenuTab(&s_NewsButton, Localize("News"), m_ActivePage==PAGE_NEWS, &Button, 0))
				NewPage = PAGE_NEWS;
			Box.VSplitLeft(30.0f, 0, &Box);
		}

		Box.VSplitLeft(100.0f, &Button, &Box);
		static int s_InternetButton=0;
		if(DoButton_MenuTab(&s_InternetButton, Localize("Internet"), m_ActivePage==PAGE_INTERNET, &Button, CUI::CORNER_TL|CUI::CORNER_IBL) && m_ActivePage!=PAGE_INTERNET)
		{
			m_pClient->m_pCamera->ChangePosition(CCamera::POS_INTERNET);
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			NewPage = PAGE_INTERNET;
		}

		//Box.VSplitLeft(4.0f, 0, &Box);
		Box.VSplitLeft(80.0f, &Button, &Box);
		static int s_LanButton=0;
		if(DoButton_MenuTab(&s_LanButton, Localize("LAN"), m_ActivePage==PAGE_LAN, &Button, 0) && m_ActivePage!=PAGE_LAN)
		{
			m_pClient->m_pCamera->ChangePosition(CCamera::POS_LAN);
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
			NewPage = PAGE_LAN;
		}

		//box.VSplitLeft(4.0f, 0, &box);
		Box.VSplitLeft(110.0f, &Button, &Box);
		static int s_FavoritesButton=0;
		if(DoButton_MenuTab(&s_FavoritesButton, Localize("Favorites"), m_ActivePage==PAGE_FAVORITES, &Button, CUI::CORNER_TR) && m_ActivePage!=PAGE_FAVORITES)
		{
			m_pClient->m_pCamera->ChangePosition(CCamera::POS_FAVORITES);
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
			NewPage = PAGE_FAVORITES;
		}

		Box.VSplitLeft(4.0f*5, 0, &Box);
		Box.VSplitLeft(100.0f, &Button, &Box);
		static int s_DemosButton=0;
		if(DoButton_MenuTab(&s_DemosButton, Localize("Demos"), m_ActivePage==PAGE_DEMOS, &Button, CUI::CORNER_T) && m_ActivePage!=PAGE_DEMOS)
		{
			m_pClient->m_pCamera->ChangePosition(CCamera::POS_DEMOS);
			DemolistPopulate();
			NewPage = PAGE_DEMOS;
		}*/

		// render menu tabs
		if(m_MenuPage >= PAGE_INTERNET && m_MenuPage <= PAGE_FRIENDS)
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_InternetButton=0;
			if(DoButton_MenuTabTop(&s_InternetButton, Localize("Internet"), m_ActivePage==PAGE_INTERNET, &Button, CUI::CORNER_T|CUI::CORNER_IBL) && m_ActivePage!=PAGE_INTERNET)
			{
				m_pClient->m_pCamera->ChangePosition(CCamera::POS_INTERNET);
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				NewPage = PAGE_INTERNET;
				g_Config.m_UiBrowserPage = PAGE_INTERNET;
			}

			//Box.VSplitLeft(4.0f, 0, &Box);
			Box.VSplitLeft(80.0f, &Button, &Box);
			static int s_LanButton=0;
			if(DoButton_MenuTabTop(&s_LanButton, Localize("LAN"), m_ActivePage==PAGE_LAN, &Button, CUI::CORNER_T) && m_ActivePage!=PAGE_LAN)
			{
				m_pClient->m_pCamera->ChangePosition(CCamera::POS_LAN);
				ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
				NewPage = PAGE_LAN;
				g_Config.m_UiBrowserPage = PAGE_LAN;
			}
		}
		else if(m_MenuPage == PAGE_SETTINGS)
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_GeneralButton=0;
			if(DoButton_MenuTabTop(&s_GeneralButton, Localize("General"), g_Config.m_UiSettingsPage==SETTINGS_GENERAL, &Button, CUI::CORNER_T|CUI::CORNER_IBL))
			{
				g_Config.m_UiSettingsPage = SETTINGS_GENERAL;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_PlayerButton=0;
			if(DoButton_MenuTabTop(&s_PlayerButton, Localize("Player"), g_Config.m_UiSettingsPage==SETTINGS_PLAYER, &Button, CUI::CORNER_T))
			{
				g_Config.m_UiSettingsPage = SETTINGS_PLAYER;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_TeeButton=0;
			if(DoButton_MenuTabTop(&s_TeeButton, Localize("Tee"), g_Config.m_UiSettingsPage==SETTINGS_TEE, &Button, CUI::CORNER_T))
			{
				g_Config.m_UiSettingsPage = SETTINGS_TEE;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_ControlsButton=0;
			if(DoButton_MenuTabTop(&s_ControlsButton, Localize("Controls"), g_Config.m_UiSettingsPage==SETTINGS_CONTROLS, &Button, CUI::CORNER_T))
			{
				g_Config.m_UiSettingsPage = SETTINGS_CONTROLS;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_GraphicsButton=0;
			if(DoButton_MenuTabTop(&s_GraphicsButton, Localize("Graphics"), g_Config.m_UiSettingsPage==SETTINGS_GRAPHICS, &Button, CUI::CORNER_T))
			{
				g_Config.m_UiSettingsPage = SETTINGS_GRAPHICS;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_SoundButton=0;
			if(DoButton_MenuTabTop(&s_SoundButton, Localize("Sound"), g_Config.m_UiSettingsPage==SETTINGS_SOUND, &Button, CUI::CORNER_T))
			{
				g_Config.m_UiSettingsPage = SETTINGS_SOUND;
			}
		}

		// back to menu
		Box.VSplitRight(90.0f, &Box, &Button);
		static int s_MenuButton=0;
		if(DoButton_Menu(&s_MenuButton, Localize("Menu"), 0, &Button, 12.0f, 0.0f, CUI::CORNER_T|CUI::CORNER_IBR))
			NewPage = PAGE_START;
	}
	else
	{
		// online menus
		if(m_GamePage != PAGE_SETTINGS) // Game stuff
		{
			Box.VSplitLeft(90.0f, &Button, &Box);
			static int s_GameButton=0;
			if(DoButton_MenuTab(&s_GameButton, Localize("Game"), m_ActivePage==PAGE_GAME, &Button, CUI::CORNER_TL|CUI::CORNER_IBL))
				NewPage = PAGE_GAME;

			Box.VSplitLeft(90.0f, &Button, &Box);
			static int s_PlayersButton=0;
			if(DoButton_MenuTab(&s_PlayersButton, Localize("Players"), m_ActivePage==PAGE_PLAYERS, &Button, 0))
				NewPage = PAGE_PLAYERS;

			Box.VSplitLeft(130.0f, &Button, &Box);
			static int s_ServerInfoButton=0;
			if(DoButton_MenuTab(&s_ServerInfoButton, Localize("Server info"), m_ActivePage==PAGE_SERVER_INFO, &Button, 0))
				NewPage = PAGE_SERVER_INFO;

			Box.VSplitLeft(130.0f, &Button, &Box);
			static int s_CallVoteButton=0;
			if(DoButton_MenuTab(&s_CallVoteButton, Localize("Call vote"), m_ActivePage==PAGE_CALLVOTE, &Button, CUI::CORNER_TR))
				NewPage = PAGE_CALLVOTE;

			Box.VSplitRight(90.0f, &Box, &Button);
			static int s_QuitButton=0;
			if(DoButton_MenuTab(&s_QuitButton, Localize("Quit"), 0, &Button, CUI::CORNER_TR|CUI::CORNER_IBR))
				m_Popup = POPUP_QUIT;

			Box.VSplitRight(130.0f, &Box, &Button);
			static int s_SettingsButton=0;
			if(DoButton_MenuTab(&s_SettingsButton, Localize("Settings"), 0, &Button, CUI::CORNER_TL))
				NewPage = PAGE_SETTINGS;
		}
		else // settings menu
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_InGameGeneralButton=0;
			if(DoButton_MenuTab(&s_InGameGeneralButton, Localize("General"), g_Config.m_UiSettingsPage==SETTINGS_GENERAL, &Button, CUI::CORNER_TL|CUI::CORNER_IBL))
				g_Config.m_UiSettingsPage = SETTINGS_GENERAL;

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_InGamePlayerButton=0;
			if(DoButton_MenuTab(&s_InGamePlayerButton, Localize("Players"), g_Config.m_UiSettingsPage==SETTINGS_PLAYER, &Button, 0))
				g_Config.m_UiSettingsPage = SETTINGS_PLAYER;

			Box.VSplitLeft(60.0f, &Button, &Box);
			static int s_InGameTeeButton=0;
			if(DoButton_MenuTab(&s_InGameTeeButton, Localize("Tee"), g_Config.m_UiSettingsPage==SETTINGS_TEE, &Button, 0))
				g_Config.m_UiSettingsPage = SETTINGS_TEE;

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_InGameControlsButton=0;
			if(DoButton_MenuTab(&s_InGameControlsButton, Localize("Controls"), g_Config.m_UiSettingsPage==SETTINGS_CONTROLS, &Button, 0))
				g_Config.m_UiSettingsPage = SETTINGS_CONTROLS;

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_InGameGraphicsButton=0;
			if(DoButton_MenuTab(&s_InGameGraphicsButton, Localize("Graphics"), g_Config.m_UiSettingsPage==SETTINGS_GRAPHICS, &Button, 0))
				g_Config.m_UiSettingsPage = SETTINGS_GRAPHICS;

			Box.VSplitLeft(90.0f, &Button, &Box);
			static int s_InGameSoundButton=0;
			if(DoButton_MenuTab(&s_InGameSoundButton, Localize("Sound"), g_Config.m_UiSettingsPage==SETTINGS_SOUND, &Button, CUI::CORNER_TR))
				g_Config.m_UiSettingsPage = SETTINGS_SOUND;

			Box.VSplitRight(90.0f, &Box, &Button);
			static int s_InGameMenuButton=0;
			if(DoButton_MenuTab(&s_InGameMenuButton, Localize("Back"), 0, &Button, CUI::CORNER_T|CUI::CORNER_IBR))
				NewPage = PAGE_GAME;
		}
	}

	if(NewPage != -1)
	{
		if(Client()->State() == IClient::STATE_OFFLINE)
			m_MenuPage = NewPage;
		else
			m_GamePage = NewPage;
	}

	return 0;
}

void CMenus::RenderLoading()
{
	// TODO: not supported right now due to separate render thread

	static int64 LastLoadRender = 0;
	float Percent = m_LoadCurrent++/(float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get()-LastLoadRender < time_freq()/60)
		return;

	LastLoadRender = time_get();

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	RenderBackground();

	float w = 700;
	float h = 200;
	float x = Screen.w/2-w/2;
	float y = Screen.h/2-h/2;

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.50f);
	RenderTools()->DrawRoundRect(x, y, w, h, 40.0f);
	Graphics()->QuadsEnd();


	const char *pCaption = Localize("Loading");

	CUIRect r;
	r.x = x;
	r.y = y+20;
	r.w = w;
	r.h = h;
	UI()->DoLabel(&r, pCaption, 48.0f, 0, -1);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,0.75f);
	RenderTools()->DrawRoundRect(x+40, y+h-75, (w-80)*Percent, 25, 5.0f);
	Graphics()->QuadsEnd();

	Graphics()->Swap();
}

void CMenus::RenderNews(CUIRect MainView)
{
	RenderTools()->DrawUIRect(&MainView, vec4(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 10.0f);
}

int CMenus::MenuImageScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	int l = str_length(pName);
	if(l < 4 || IsDir || str_comp(pName+l-4, ".png") != 0)
		return 0;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "menuimages/%s", pName);
	CImageInfo Info;
	if(!pSelf->Graphics()->LoadPNG(&Info, aBuf, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load menu image from %s", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
		return 0;
	}

	CMenuImage MenuImage;
	MenuImage.m_OrgTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);

	unsigned char *d = (unsigned char *)Info.m_pData;
	//int Pitch = Info.m_Width*4;

	// create colorless version
	int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

	// make the texture gray scale
	for(int i = 0; i < Info.m_Width*Info.m_Height; i++)
	{
		int v = (d[i*Step]+d[i*Step+1]+d[i*Step+2])/3;
		d[i*Step] = v;
		d[i*Step+1] = v;
		d[i*Step+2] = v;
	}

	/* same grey like sinks
	int Freq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// find most common frequence
	for(int y = 0; y < Info.m_Height; y++)
		for(int x = 0; x < Info.m_Width; x++)
		{
			if(d[y*Pitch+x*4+3] > 128)
				Freq[d[y*Pitch+x*4]]++;
		}

	for(int i = 1; i < 256; i++)
	{
		if(Freq[OrgWeight] < Freq[i])
			OrgWeight = i;
	}

	// reorder
	int InvOrgWeight = 255-OrgWeight;
	int InvNewWeight = 255-NewWeight;
	for(int y = 0; y < Info.m_Height; y++)
		for(int x = 0; x < Info.m_Width; x++)
		{
			int v = d[y*Pitch+x*4];
			if(v <= OrgWeight)
				v = (int)(((v/(float)OrgWeight) * NewWeight));
			else
				v = (int)(((v-OrgWeight)/(float)InvOrgWeight)*InvNewWeight + NewWeight);
			d[y*Pitch+x*4] = v;
			d[y*Pitch+x*4+1] = v;
			d[y*Pitch+x*4+2] = v;
		}
	*/

	MenuImage.m_GreyTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);

	// set menu image data
	str_copy(MenuImage.m_aName, pName, min((int)sizeof(MenuImage.m_aName),l-3));
	str_format(aBuf, sizeof(aBuf), "load menu image %s", MenuImage.m_aName);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
	pSelf->m_lMenuImages.add(MenuImage);

	return 0;
}

const CMenus::CMenuImage *CMenus::FindMenuImage(const char *pName)
{
	for(int i = 0; i < m_lMenuImages.size(); i++)
	{
		if(str_comp(m_lMenuImages[i].m_aName, pName) == 0)
			return &m_lMenuImages[i];
	}
	return 0;
}

void CMenus::OnInit()
{
	// load menu images
	m_lMenuImages.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "menuimages", MenuImageScan, this);

	/*
	array<string> my_strings;
	array<string>::range r2;
	my_strings.add("4");
	my_strings.add("6");
	my_strings.add("1");
	my_strings.add("3");
	my_strings.add("7");
	my_strings.add("5");
	my_strings.add("2");

	for(array<string>::range r = my_strings.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%s", r.front().cstr());

	sort(my_strings.all());

	dbg_msg("", "after:");
	for(array<string>::range r = my_strings.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%s", r.front().cstr());


	array<int> myarray;
	myarray.add(4);
	myarray.add(6);
	myarray.add(1);
	myarray.add(3);
	myarray.add(7);
	myarray.add(5);
	myarray.add(2);

	for(array<int>::range r = myarray.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%d", r.front());

	sort(myarray.all());
	sort_verify(myarray.all());

	dbg_msg("", "after:");
	for(array<int>::range r = myarray.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%d", r.front());

	exit(-1);
	// */

	// clear filter lists
	//m_lFilters.clear();

	if(g_Config.m_ClShowWelcome)
		m_Popup = POPUP_LANGUAGE;
	g_Config.m_ClShowWelcome = 0;

	Console()->Chain("add_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("remove_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("add_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("remove_friend", ConchainFriendlistUpdate, this);

	m_TextureBlob = Graphics()->LoadTexture("blob.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	// setup load amount
	m_LoadCurrent = 0;
	m_LoadTotal = g_pData->m_NumImages;
	if(!g_Config.m_ClThreadsoundloading)
		m_LoadTotal += g_pData->m_NumSounds;
}

void CMenus::PopupMessage(const char *pTopic, const char *pBody, const char *pButton)
{
	// reset active item
	UI()->SetActiveItem(0);

	str_copy(m_aMessageTopic, pTopic, sizeof(m_aMessageTopic));
	str_copy(m_aMessageBody, pBody, sizeof(m_aMessageBody));
	str_copy(m_aMessageButton, pButton, sizeof(m_aMessageButton));
	m_Popup = POPUP_MESSAGE;
}


int CMenus::Render()
{
	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	static bool s_First = true;
	if(s_First)
	{
		// refresh server browser before we are in browser menu to save time
		m_pClient->m_pCamera->ChangePosition(CCamera::POS_START);
		if(g_Config.m_UiBrowserPage == PAGE_LAN)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		else
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);

		m_pClient->m_pSounds->Enqueue(CSounds::CHN_MUSIC, SOUND_MENU);
		s_First = false;
	}

	// render background only if needed
	if(Client()->State() != IClient::STATE_ONLINE && !m_pClient->m_pMapLayersBackGround->MenuMapLoaded())
		RenderBackground();

	CUIRect TabBar;
	CUIRect MainView;

	// some margin around the screen
	Screen.Margin(10.0f, &Screen);

	static bool s_SoundCheck = false;
	if(!s_SoundCheck && m_Popup == POPUP_NONE)
	{
		if(Client()->SoundInitFailed())
			m_Popup = POPUP_SOUNDERROR;
		s_SoundCheck = true;
	}

	if(m_Popup == POPUP_NONE)
	{
		if(m_MenuPage == PAGE_START && Client()->State() == IClient::STATE_OFFLINE)
		{
			RenderLogo(Screen);
			RenderStartMenu(Screen);
		}
		else
		{
			// do tab bar
			Screen.VMargin(Screen.w/2-365.0f, &MainView);
			MainView.HMargin(20.0f, &MainView);
			MainView.HSplitTop(24.0f, &TabBar, &MainView);
			RenderMenubar(TabBar);

			// news is not implemented yet
			/*if(m_MenuPage <= PAGE_NEWS || m_MenuPage > PAGE_SETTINGS || (Client()->State() == IClient::STATE_OFFLINE && m_MenuPage >= PAGE_GAME && m_MenuPage <= PAGE_CALLVOTE))
			{
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				m_MenuPage = PAGE_INTERNET;
			}*/

			// render current page
			if(Client()->State() != IClient::STATE_OFFLINE)
			{
				if(m_GamePage == PAGE_GAME)
					RenderGame(MainView);
				else if(m_GamePage == PAGE_PLAYERS)
					RenderPlayers(MainView);
				else if(m_GamePage == PAGE_SERVER_INFO)
					RenderServerInfo(MainView);
				else if(m_GamePage == PAGE_CALLVOTE)
					RenderServerControl(MainView);
				else if(m_GamePage == PAGE_SETTINGS)
					RenderSettings(MainView);
			}
			else
			{
				if(m_MenuPage == PAGE_NEWS)
					RenderNews(MainView);
				else if(m_MenuPage == PAGE_INTERNET)
					RenderServerbrowser(MainView);
				else if(m_MenuPage == PAGE_LAN)
					RenderServerbrowser(MainView);
				else if(m_MenuPage == PAGE_DEMOS)
					RenderDemoList(MainView);
				else if(m_MenuPage == PAGE_FRIENDS)
					RenderServerbrowser(MainView);
				else if(m_MenuPage == PAGE_SETTINGS)
					RenderSettings(MainView);

				// handle back with esc
				if(m_EscapePressed)
					m_MenuPage = PAGE_START;
			}
		}

		// do overlay popups
		DoPopupMenu();
	}
	else
	{
		// make sure that other windows doesn't do anything funnay!
		//UI()->SetHotItem(0);
		//UI()->SetActiveItem(0);
		char aBuf[128];
		const char *pTitle = "";
		const char *pExtraText = "";
		const char *pButtonText = "";
		int ExtraAlign = 0;

		if(m_Popup == POPUP_MESSAGE)
		{
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			pTitle = Localize("Connecting to");
			pExtraText = g_Config.m_UiServerAddress; // TODO: query the client about the address
			pButtonText = Localize("Abort");
			if(Client()->MapDownloadTotalsize() > 0)
			{
				pTitle = Localize("Downloading map");
				pExtraText = "";
			}
		}
		else if(m_Popup == POPUP_DISCONNECTED)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Client()->ErrorString();
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_PURE)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Localize("The server is running a non-standard tuning on a pure game type.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			pTitle = Localize("Delete demo");
			pExtraText = Localize("Are you sure that you want to delete the demo?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			pTitle = Localize("Rename demo");
			pExtraText = "";
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			pTitle = Localize("Remove friend");
			pExtraText = Localize("Are you sure that you want to remove the player from your friends list?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SAVE_SKIN)
		{
			pTitle = Localize("Save skin");
			pExtraText = Localize("Are you sure you want to save your skin ? If a skin with this name already exists, it will be replaced.");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SOUNDERROR)
		{
			pTitle = Localize("Sound error");
			pExtraText = Localize("The audio device couldn't be initialised.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			pTitle = Localize("Password incorrect");
			pExtraText = "";
			pButtonText = Localize("Try again");
		}
		else if(m_Popup == POPUP_QUIT)
		{
			pTitle = Localize("Quit");
			pExtraText = Localize("Are you sure that you want to quit?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			pTitle = Localize("Welcome to Teeworlds");
			pExtraText = Localize("As this is the first time you launch the game, please enter your nick name below. It's recommended that you check the settings to adjust them to your liking before joining a server.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}

		CUIRect Box, Part;
		Box = Screen;
		Box.VMargin(150.0f/UI()->Scale(), &Box);
		Box.HMargin(150.0f/UI()->Scale(), &Box);

		// render the box
		RenderTools()->DrawUIRect(&Box, vec4(0,0,0,0.5f), CUI::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f/UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f/UI()->Scale(), &Part, &Box);
		UI()->DoLabelScaled(&Part, pTitle, 24.f, 0);
		Box.HSplitTop(20.f/UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f/UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f/UI()->Scale(), &Part);

		if(ExtraAlign == -1)
			UI()->DoLabelScaled(&Part, pExtraText, 20.f, -1, (int)Part.w);
		else
			UI()->DoLabelScaled(&Part, pExtraText, 20.f, 0, -1);

		if(m_Popup == POPUP_QUIT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// additional info
			Box.HSplitTop(10.0f, 0, &Box);
			Box.VMargin(20.f/UI()->Scale(), &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s\n%s", Localize("There's an unsaved map in the editor, you might want to save it before you quit the game."), Localize("Quit anyway?"));
				UI()->DoLabelScaled(&Box, aBuf, 20.f, -1, Part.w-20.0f);
			}

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
				Client()->Quit();
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			CUIRect Label, TextBox, TryAgain, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &TryAgain);

			TryAgain.VMargin(20.0f, &TryAgain);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Try again"), 0, &TryAgain) || m_EnterPressed)
			{
				Client()->Connect(g_Config.m_UiServerAddress);
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_Password, &TextBox, g_Config.m_Password, sizeof(g_Config.m_Password), 12.0f, &Offset, true);
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
			{
				Client()->Disconnect();
				m_Popup = POPUP_NONE;
			}

			if(Client()->MapDownloadTotalsize() > 0)
			{
				int64 Now = time_get();
				if(Now-m_DownloadLastCheckTime >= time_freq())
				{
					if(m_DownloadLastCheckSize > Client()->MapDownloadAmount())
					{
						// map downloaded restarted
						m_DownloadLastCheckSize = 0;
					}

					// update download speed
					float Diff = (Client()->MapDownloadAmount()-m_DownloadLastCheckSize)/((int)((Now-m_DownloadLastCheckTime)/time_freq()));
					float StartDiff = m_DownloadLastCheckSize-0.0f;
					if(StartDiff+Diff > 0.0f)
						m_DownloadSpeed = (Diff/(StartDiff+Diff))*(Diff/1.0f) + (StartDiff/(Diff+StartDiff))*m_DownloadSpeed;
					else
						m_DownloadSpeed = 0.0f;
					m_DownloadLastCheckTime = Now;
					m_DownloadLastCheckSize = Client()->MapDownloadAmount();
				}

				Box.HSplitTop(64.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf, sizeof(aBuf), "%d/%d KiB (%.1f KiB/s)", Client()->MapDownloadAmount()/1024, Client()->MapDownloadTotalsize()/1024,	m_DownloadSpeed/1024.0f);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// time left
				const char *pTimeLeftString;
				int TimeLeft = max(1, m_DownloadSpeed > 0.0f ? static_cast<int>((Client()->MapDownloadTotalsize()-Client()->MapDownloadAmount())/m_DownloadSpeed) : 1);
				if(TimeLeft >= 60)
				{
					TimeLeft /= 60;
					pTimeLeftString = TimeLeft == 1 ? Localize("%i minute left") : Localize("%i minutes left");
				}
				else
					pTimeLeftString = TimeLeft == 1 ? Localize("%i second left") : Localize("%i seconds left");
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf, sizeof(aBuf), pTimeLeftString, TimeLeft);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// progress bar
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				Part.VMargin(40.0f, &Part);
				RenderTools()->DrawUIRect(&Part, vec4(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 5.0f);
				Part.w = max(10.0f, (Part.w*Client()->MapDownloadAmount())/Client()->MapDownloadTotalsize());
				RenderTools()->DrawUIRect(&Part, vec4(1.0f, 1.0f, 1.0f, 0.5f), CUI::CORNER_ALL, 5.0f);
			}
		}
		else if(m_Popup == POPUP_LANGUAGE)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);
			RenderLanguageSelection(Box);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EscapePressed || m_EnterPressed)
				m_Popup = POPUP_FIRST_LAUNCH;
		}
		else if(m_Popup == POPUP_COUNTRY)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);

			// selected filter
			CBrowserFilter *pFilter = &m_lFilters[m_SelectedFilter];
			int SortHash = 0;
			int Ping = 0;
			int Country = 0;
			char aGametype[32];
			char aServerAddress[16];
			pFilter->GetFilter(&SortHash, &Ping, &Country, aGametype, aServerAddress);

			static int ActSelection = -2;
			if(ActSelection == -2)
				ActSelection = Country;
			static float s_ScrollValue = 0.0f;
			int OldSelected = -1;
			UiDoListboxStart(&s_ScrollValue, &Box, 50.0f, Localize("Country"), "", m_pClient->m_pCountryFlags->Num(), 6, OldSelected, s_ScrollValue);

			for(int i = 0; i < m_pClient->m_pCountryFlags->Num(); ++i)
			{
				const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_pCountryFlags->GetByIndex(i);
				if(pEntry->m_CountryCode == ActSelection)
					OldSelected = i;

				CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected == i);
				if(Item.m_Visible)
				{
					CUIRect Label;
					Item.m_Rect.Margin(5.0f, &Item.m_Rect);
					Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
					float OldWidth = Item.m_Rect.w;
					Item.m_Rect.w = Item.m_Rect.h*2;
					Item.m_Rect.x += (OldWidth-Item.m_Rect.w)/ 2.0f;
					vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
					m_pClient->m_pCountryFlags->Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
					UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
				}
			}

			const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
			if(OldSelected != NewSelected)
				ActSelection = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;

			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EnterPressed)
			{
				pFilter->SetFilter(SortHash, Ping, ActSelection, aGametype, aServerAddress);
				m_Popup = POPUP_NONE;
			}

			if(m_EscapePressed)
			{
				ActSelection = Country;
				m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// delete demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					if(Storage()->RemoveFile(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to delete the demo"), Localize("Ok"));
				}
			}
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// rename demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 4 || m_aCurrentDemoFile[Length-5] != '.' || str_comp_nocase(m_aCurrentDemoFile+Length-4, "demo"))
						str_format(aBufNew, sizeof(aBufNew), "%s/%s.demo", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					if(Storage()->RenameFile(aBufOld, aBufNew, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to rename the demo"), Localize("Ok"));
				}
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset);
		}
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// remove friend
				if(m_FriendlistSelectedIndex >= 0)
				{
					m_pClient->Friends()->RemoveFriend(m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName,
						m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aClan);
					FriendlistOnUpdate();
					Client()->ServerBrowserUpdate();
				}
			}
		}
		else if(m_Popup == POPUP_SAVE_SKIN)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				SaveSkinfile();
				m_aSaveSkinName[0] = 0;
			}
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			CUIRect Label, TextBox;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			static int s_EnterButton = 0;
			if(DoButton_Menu(&s_EnterButton, Localize("Enter"), 0, &Part) || m_EnterPressed)
				m_Popup = POPUP_NONE;

			Box.HSplitBottom(40.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Nickname"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_PlayerName, &TextBox, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 12.0f, &Offset);
		}
		else
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
				m_Popup = POPUP_NONE;
		}
	}

	return 0;
}


void CMenus::SetActive(bool Active)
{
	m_MenuActive = Active;
	if(!m_MenuActive)
	{
		if(Client()->State() == IClient::STATE_ONLINE)
		{
			m_pClient->OnRelease();
		}
	}
	else if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->OnRelease();
	}
}

void CMenus::OnReset()
{
}

bool CMenus::OnMouseMove(float x, float y)
{
	m_LastInput = time_get();

	if(!m_MenuActive)
		return false;

	UI()->ConvertMouseMove(&x, &y);
	m_MousePos.x += x;
	m_MousePos.y += y;
	if(m_MousePos.x < 0) m_MousePos.x = 0;
	if(m_MousePos.y < 0) m_MousePos.y = 0;
	if(m_MousePos.x > Graphics()->ScreenWidth()) m_MousePos.x = Graphics()->ScreenWidth();
	if(m_MousePos.y > Graphics()->ScreenHeight()) m_MousePos.y = Graphics()->ScreenHeight();

	return true;
}

bool CMenus::OnInput(IInput::CEvent e)
{
	m_LastInput = time_get();

	// special handle esc and enter for popup purposes
	if(e.m_Flags&IInput::FLAG_PRESS)
	{
		if(e.m_Key == KEY_ESCAPE)
		{
			m_EscapePressed = true;
			SetActive(!IsActive());
			return true;
		}
	}

	if(IsActive())
	{
		if(e.m_Flags&IInput::FLAG_PRESS)
		{
			// special for popups
			if(e.m_Key == KEY_RETURN || e.m_Key == KEY_KP_ENTER)
				m_EnterPressed = true;
			else if(e.m_Key == KEY_DELETE)
				m_DeletePressed = true;
		}

		if(m_NumInputEvents < MAX_INPUTEVENTS)
			m_aInputEvents[m_NumInputEvents++] = e;
		return true;
	}
	return false;
}

void CMenus::OnConsoleInit()
{
	// add filters
	m_lFilters.add(CBrowserFilter(CBrowserFilter::FILTER_ALL, Localize("All"), ServerBrowser(), IServerBrowser::FILTER_PING, 999, -1, "", ""));
	m_lFilters.add(CBrowserFilter(CBrowserFilter::FILTER_STANDARD, Localize("Standard Gametype"), ServerBrowser(), IServerBrowser::FILTER_COMPAT_VERSION|IServerBrowser::FILTER_PURE|IServerBrowser::FILTER_PURE_MAP|IServerBrowser::FILTER_PING, 999, -1, "", ""));
	m_lFilters.add(CBrowserFilter(CBrowserFilter::FILTER_FAVORITES, Localize("Favorites"), ServerBrowser(), IServerBrowser::FILTER_FAVORITE|IServerBrowser::FILTER_PING, 999, -1, "", ""));
}

void CMenus::OnStateChange(int NewState, int OldState)
{
	// reset active item
	UI()->SetActiveItem(0);

	if(NewState == IClient::STATE_OFFLINE)
	{
		if(OldState >= IClient::STATE_ONLINE && NewState < IClient::STATE_QUITING)
			m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		m_Popup = POPUP_NONE;
		if(Client()->ErrorString() && Client()->ErrorString()[0] != 0)
		{
			if(str_find(Client()->ErrorString(), "password"))
			{
				m_Popup = POPUP_PASSWORD;
				UI()->SetHotItem(&g_Config.m_Password);
				UI()->SetActiveItem(&g_Config.m_Password);
			}
			else
				m_Popup = POPUP_DISCONNECTED;
		}
	}
	else if(NewState == IClient::STATE_LOADING)
	{
		m_Popup = POPUP_CONNECTING;
		m_DownloadLastCheckTime = time_get();
		m_DownloadLastCheckSize = 0;
		m_DownloadSpeed = 0.0f;
		//client_serverinfo_request();
	}
	else if(NewState == IClient::STATE_CONNECTING)
		m_Popup = POPUP_CONNECTING;
	else if (NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		m_Popup = POPUP_NONE;
		SetActive(false);
	}
}

extern "C" void font_debug_render();

void CMenus::OnRender()
{
	/*
	// text rendering test stuff
	render_background();

	CTextCursor cursor;
	TextRender()->SetCursor(&cursor, 10, 10, 20, TEXTFLAG_RENDER);
	TextRender()->TextEx(&cursor, "ようこそ - ガイド", -1);

	TextRender()->SetCursor(&cursor, 10, 30, 15, TEXTFLAG_RENDER);
	TextRender()->TextEx(&cursor, "ようこそ - ガイド", -1);

	//Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->QuadsDrawTL(60, 60, 5000, 5000);
	Graphics()->QuadsEnd();
	return;*/

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		SetActive(true);

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
		RenderDemoPlayer(Screen);
	}

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_ServerMode == m_pClient->SERVERMODE_PUREMOD)
	{
		Client()->Disconnect();
		SetActive(true);
		m_Popup = POPUP_PURE;
	}

	if(!IsActive())
	{
		m_EscapePressed = false;
		m_EnterPressed = false;
		m_DeletePressed = false;
		m_NumInputEvents = 0;
		return;
	}

	// update the ui
	CUIRect *pScreen = UI()->Screen();
	float mx = (m_MousePos.x/(float)Graphics()->ScreenWidth())*pScreen->w;
	float my = (m_MousePos.y/(float)Graphics()->ScreenHeight())*pScreen->h;

	int Buttons = 0;
	if(m_UseMouseButtons)
	{
		if(Input()->KeyPressed(KEY_MOUSE_1)) Buttons |= 1;
		if(Input()->KeyPressed(KEY_MOUSE_2)) Buttons |= 2;
		if(Input()->KeyPressed(KEY_MOUSE_3)) Buttons |= 4;
	}

	UI()->Update(mx,my,mx*3.0f,my*3.0f,Buttons);

	// render
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Render();

	// render cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(mx, my, 24, 24);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render debug information
	if(g_Config.m_Debug)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%p %p %p", UI()->HotItem(), UI()->ActiveItem(), UI()->LastActiveItem());
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 10, 10, 10, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);
	}

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;
}

void CMenus::RenderBackground()
{
	//Graphics()->Clear(1,1,1);
	//render_sunrays(0,0);

	float sw = 300*Graphics()->ScreenAspect();
	float sh = 300;
	Graphics()->MapScreen(0, 0, sw, sh);

	// render background color
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
		//vec4 bottom(gui_color.r*0.3f, gui_color.g*0.3f, gui_color.b*0.3f, 1.0f);
		//vec4 bottom(0, 0, 0, 1.0f);
		vec4 Bottom(0.25f, 0.25f, 0.25f, 1.0f);
		vec4 Top(0.25f, 0.25f, 0.25f, 1.0f);
		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
			IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
		Graphics()->SetColorVertex(Array, 4);
		IGraphics::CQuadItem QuadItem(0, 0, sw, sh);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render the tiles
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
		float Size = 15.0f;
		float OffsetTime = fmod(Client()->LocalTime()*0.15f, 2.0f);
		for(int y = -2; y < (int)(sw/Size); y++)
			for(int x = -2; x < (int)(sh/Size); x++)
			{
				Graphics()->SetColor(0,0,0,0.045f);
				IGraphics::CQuadItem QuadItem((x-OffsetTime)*Size*2+(y&1)*Size, (y+OffsetTime)*Size, Size, Size);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
	Graphics()->QuadsEnd();

	// render border fade
	Graphics()->TextureSet(m_TextureBlob);
	Graphics()->QuadsBegin();
		Graphics()->SetColor(0,0,0,0.5f);
		QuadItem = IGraphics::CQuadItem(-100, -100, sw+200, sh+200);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// restore screen
	{CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);}
}
