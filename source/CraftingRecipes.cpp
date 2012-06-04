
// CraftingRecipes.cpp

// Interfaces to the cCraftingRecipes class representing the storage of crafting recipes

#include "Globals.h"
#include "CraftingRecipes.h"





cCraftingRecipes::cCraftingRecipes(void)
{
	LoadRecipes();
}





cCraftingRecipes::~cCraftingRecipes()
{
	ClearRecipes();
}





/// Offers an item resulting from the crafting grid setup. Doesn't modify the grid
cItem cCraftingRecipes::Offer(const cItem * a_CraftingGrid, int a_GridWidth, int a_GridHeight)
{
	std::auto_ptr<cRecipe> Recipe(FindRecipe(a_CraftingGrid, a_GridWidth, a_GridHeight));
	if (Recipe.get() == NULL)
	{
		return cItem();
	}
	
	return Recipe->m_Result;
}





/// Crafts the item resulting from the crafting grid setup. Modifies the grid, returns the crafted item
cItem cCraftingRecipes::Craft(cItem * a_CraftingGrid, int a_GridWidth, int a_GridHeight)
{
	std::auto_ptr<cRecipe> Recipe(FindRecipe(a_CraftingGrid, a_GridWidth, a_GridHeight));
	if (Recipe.get() == NULL)
	{
		return cItem();
	}
	
	// Consume the ingredients from the grid:
	for (cRecipeSlots::const_iterator itr = Recipe->m_Ingredients.begin(); itr != Recipe->m_Ingredients.end(); ++itr)
	{
		int GridIdx = itr->x + a_GridWidth * itr->y;
		a_CraftingGrid[GridIdx].m_ItemCount -= itr->m_Item.m_ItemCount;
		if (a_CraftingGrid[GridIdx].m_ItemCount <= 0)
		{
			a_CraftingGrid[GridIdx].Empty();
		}
	}
	return Recipe->m_Result;
}





void cCraftingRecipes::LoadRecipes(void)
{
	LOG("-- Loading crafting recipes from crafting.txt --");
	ClearRecipes();
	
	// Load the crafting.txt file:
	cFile f;
	if (!f.Open("crafting.txt", cFile::fmRead))
	{
		LOGWARNING("Cannot open file \"crafting.txt\", no crafting recipes will be available!");
		return;
	}
	AString Everything;
	f.ReadRestOfFile(Everything);
	f.Close();
	
	// Split it into lines, then process each line as a single recipe:
	AStringVector Split = StringSplit(Everything, "\n");
	int LineNum = 1;
	for (AStringVector::const_iterator itr = Split.begin(); itr != Split.end(); ++itr, ++LineNum)
	{
		// Remove anything after a '#' sign and trim away the whitespace:
		AString Recipe = TrimString(itr->substr(0, itr->find('#')));
		if (Recipe.empty())
		{
			// Empty recipe
			continue;
		}
		AddRecipeLine(LineNum, Recipe);
	}  // for itr - Split[]
	LOG("-- %d crafting recipes loaded from crafting.txt --", m_Recipes.size());
}




void cCraftingRecipes::ClearRecipes(void)
{
	for (cRecipes::iterator itr = m_Recipes.begin(); itr != m_Recipes.end(); ++itr)
	{
		delete *itr;
	}
	m_Recipes.clear();
}





void cCraftingRecipes::AddRecipeLine(int a_LineNum, const AString & a_RecipeLine)
{
	AStringVector Sides = StringSplit(a_RecipeLine, "=");
	if (Sides.size() != 2)
	{
		LOGWARNING("crafting.txt: line %d: A single '=' was expected, got %d", a_LineNum, (int)Sides.size() - 1);
		return;
	}
	
	std::auto_ptr<cCraftingRecipes::cRecipe> Recipe(new cCraftingRecipes::cRecipe);
	
	// Parse the result:
	AStringVector ResultSplit = StringSplit(Sides[0], ",");
	if (ResultSplit.empty())
	{
		LOGWARNING("crafting.txt: line %d: Result is empty, ignoring the recipe.", a_LineNum);
		return;
	}
	if (!ParseItem(ResultSplit[0], Recipe->m_Result))
	{
		LOGWARNING("crafting.txt: line %d: Cannot parse result item, ignoring the recipe.", a_LineNum);
		return;
	}
	if (ResultSplit.size() > 1)
	{
		Recipe->m_Result.m_ItemCount = atoi(ResultSplit[1].c_str());
		if (Recipe->m_Result.m_ItemCount == 0)
		{
			LOGWARNING("crafting.txt: line %d: Cannot parse result count, ignoring the recipe.", a_LineNum);
			return;
		}
	}
	else
	{
		Recipe->m_Result.m_ItemCount = 1;
	}
	
	// Parse each ingredient:
	AStringVector Ingredients = StringSplit(Sides[1], "|");
	int Num = 1;
	for (AStringVector::const_iterator itr = Ingredients.begin(); itr != Ingredients.end(); ++itr, ++Num)
	{
		if (!ParseIngredient(*itr, Recipe.get()))
		{
			LOGWARNING("crafting.txt: line %d: Cannot parse ingredient #%d, ignoring the recipe.", a_LineNum, Num);
			return;
		}
	}  // for itr - Ingredients[]
	
	NormalizeIngredients(Recipe.get());
	
	m_Recipes.push_back(Recipe.release());
}





bool cCraftingRecipes::ParseItem(const AString & a_String, cItem & a_Item)
{
	// The caller provides error logging
	
	AStringVector Split = StringSplit(a_String, "^");
	if (Split.empty())
	{
		return false;
	}
	
	if (!StringToItem(Split[0], a_Item))
	{
		return false;
	}
	
	if (Split.size() > 1)
	{
		AString Damage = TrimString(Split[1]);
		a_Item.m_ItemHealth = atoi(Damage.c_str());
		if ((a_Item.m_ItemHealth == 0) && (Damage.compare("0") != 0))
		{
			// Parsing the number failed
			return false;
		}
	}
	
	// Success
	return true;
}





bool cCraftingRecipes::ParseIngredient(const AString & a_String, cRecipe * a_Recipe)
{
	// a_String is in this format: "ItemType^damage, X:Y, X:Y, X:Y..."
	AStringVector Split = StringSplit(a_String, ",");
	if (Split.size() < 2)
	{
		// Not enough split items
		return false;
	}
	cItem Item;
	if (!ParseItem(Split[0], Item))
	{
		return false;
	}
	Item.m_ItemCount = 1;
	
	cCraftingRecipes::cRecipeSlots TempSlots;
	for (AStringVector::const_iterator itr = Split.begin() + 1; itr != Split.end(); ++itr)
	{
		// Parse the coords in the split item:
		AStringVector Coords = StringSplit(*itr, ":");
		if ((Coords.size() == 1) && (TrimString(Coords[0]) == "*"))
		{
			cCraftingRecipes::cRecipeSlot Slot;
			Slot.m_Item = Item;
			Slot.x = -1;
			Slot.y = -1;
			TempSlots.push_back(Slot);
			continue;
		}
		if (Coords.size() != 2)
		{
			return false;
		}
		Coords[0] = TrimString(Coords[0]);
		Coords[1] = TrimString(Coords[1]);
		if (Coords[0].empty() || Coords[1].empty())
		{
			return false;
		}
		cCraftingRecipes::cRecipeSlot Slot;
		Slot.m_Item = Item;
		switch (Coords[0][0])
		{
			case '1': Slot.x = 0;  break;
			case '2': Slot.x = 1;  break;
			case '3': Slot.x = 2;  break;
			case '*': Slot.x = -1; break;
			default:
			{
				return false;
			}
		}
		switch (Coords[1][0])
		{
			case '1': Slot.y = 0;  break;
			case '2': Slot.y = 1;  break;
			case '3': Slot.y = 2;  break;
			case '*': Slot.y = -1; break;
			default:
			{
				return false;
			}
		}
		TempSlots.push_back(Slot);
	}  // for itr - Split[]
	
	// Append the ingredients:
	a_Recipe->m_Ingredients.insert(a_Recipe->m_Ingredients.end(), TempSlots.begin(), TempSlots.end());
	return true;
}





void cCraftingRecipes::NormalizeIngredients(cCraftingRecipes::cRecipe * a_Recipe)
{
	// Calculate the minimum coords for ingredients, excluding the "anywhere" items:
	int MinX = MAX_GRID_WIDTH, MaxX = 0;
	int MinY = MAX_GRID_HEIGHT, MaxY = 0;
	for (cRecipeSlots::const_iterator itr = a_Recipe->m_Ingredients.begin(); itr != a_Recipe->m_Ingredients.end(); ++itr)
	{
		if (itr->x >= 0)
		{
			MinX = std::min(itr->x, MinX);
			MaxX = std::max(itr->x, MaxX);
		}
		if (itr->y >= 0)
		{
			MinY = std::min(itr->y, MinY);
			MaxY = std::max(itr->y, MaxY);
		}
	}  // for itr - a_Recipe->m_Ingredients[]

	// Move ingredients so that the minimum coords are 0:0
	for (cRecipeSlots::iterator itr = a_Recipe->m_Ingredients.begin(); itr != a_Recipe->m_Ingredients.end(); ++itr)
	{
		if (itr->x >= 0)
		{
			itr->x -= MinX;
		}
		if (itr->y >= 0)
		{
			itr->y -= MinY;
		}
	}  // for itr - a_Recipe->m_Ingredients[]
	a_Recipe->m_Width  = std::max(MaxX - MinX + 1, 1);
	a_Recipe->m_Height = std::max(MaxY - MinY + 1, 1);
	
	// TODO: Compress two same ingredients with the same coords into a single ingredient with increased item count
}





cCraftingRecipes::cRecipe * cCraftingRecipes::FindRecipe(const cItem * a_CraftingGrid, int a_GridWidth, int a_GridHeight)
{
	ASSERT(a_GridWidth <= MAX_GRID_WIDTH);
	ASSERT(a_GridHeight <= MAX_GRID_HEIGHT);
	
	// Get the real bounds of the crafting grid:
	int GridLeft = MAX_GRID_WIDTH, GridTop = MAX_GRID_HEIGHT;
	int GridRight = 0,  GridBottom = 0;
	for (int y = 0; y < a_GridHeight; y++ ) for(int x = 0; x < a_GridWidth; x++)
	{
		if (!a_CraftingGrid[x + y * a_GridWidth].IsEmpty())
		{
			GridRight  = MAX(x, GridRight);
			GridBottom = MAX(y, GridBottom);
			GridLeft   = MIN(x, GridLeft);
			GridTop    = MIN(y, GridTop);
		}
	}
	int GridWidth = GridRight - GridLeft + 1;
	int GridHeight = GridBottom - GridTop + 1;
	
	// Search in the possibly minimized grid, but keep the stride:
	const cItem * Grid = a_CraftingGrid + GridLeft + (a_GridWidth * GridTop);
	cRecipe * Recipe = FindRecipeCropped(Grid, GridWidth, GridHeight, a_GridWidth);
	if (Recipe == NULL)
	{
		return NULL;
	}
	
	// A recipe has been found, move it to correspond to the original crafting grid:
	for (cRecipeSlots::iterator itrS = Recipe->m_Ingredients.begin(); itrS != Recipe->m_Ingredients.end(); ++itrS)
	{
		itrS->x += GridLeft;
		itrS->y += GridTop;
	}  // for itrS - Recipe->m_Ingredients[]
	
	return Recipe;
}





cCraftingRecipes::cRecipe * cCraftingRecipes::FindRecipeCropped(const cItem * a_CraftingGrid, int a_GridWidth, int a_GridHeight, int a_GridStride)
{
	for (cRecipes::const_iterator itr = m_Recipes.begin(); itr != m_Recipes.end(); ++itr)
	{
		// Both the crafting grid and the recipes are normalized. The only variable possible is the "anywhere" items.
		// This still means that the "anywhere" item may be the one that is offsetting the grid contents to the right or downwards, so we need to check all possible positions.
		// E. g. recipe "A, * | B, 1:1 | ..." still needs to check grid for B at 2:2 (in case A was in grid's 1:1)
		// Calculate the maximum offsets for this recipe relative to the grid size, and iterate through all combinations of offsets.
		// Also, this calculation automatically filters out recipes that are too large for the current grid - the loop won't be entered at all.
		
		int MaxOfsX = a_GridWidth  - (*itr)->m_Width;
		int MaxOfsY = a_GridHeight - (*itr)->m_Height;
		for (int x = 0; x <= MaxOfsX; x++) for (int y = 0; y <= MaxOfsY; y++)
		{
			cRecipe * Recipe = MatchRecipe(a_CraftingGrid, a_GridWidth, a_GridHeight, a_GridStride, *itr, x, y);
			if (Recipe != NULL)
			{
				return Recipe;
			}
		}  // for y, for x
	}  // for itr - m_Recipes[]
	
	// No matching recipe found
	return NULL;
}





cCraftingRecipes::cRecipe * cCraftingRecipes::MatchRecipe(const cItem * a_CraftingGrid, int a_GridWidth, int a_GridHeight, int a_GridStride, const cRecipe * a_Recipe, int a_OffsetX, int a_OffsetY)
{
	// Check the regular items first:
	bool HasMatched[MAX_GRID_WIDTH][MAX_GRID_HEIGHT];
	memset(HasMatched, 0, sizeof(HasMatched));
	for (cRecipeSlots::const_iterator itrS = a_Recipe->m_Ingredients.begin(); itrS != a_Recipe->m_Ingredients.end(); ++itrS)
	{
		if ((itrS->x < 0) || (itrS->y < 0))
		{
			// "Anywhere" item, process later
			continue;
		}
		ASSERT(itrS->x + a_OffsetX < a_GridWidth);
		ASSERT(itrS->y + a_OffsetY < a_GridHeight);
		int GridID = (itrS->x + a_OffsetX) + a_GridStride * (itrS->y + a_OffsetY);
		if (
			(itrS->x >= a_GridWidth) || 
			(itrS->y >= a_GridHeight) ||
			(itrS->m_Item.m_ItemID != a_CraftingGrid[GridID].m_ItemID) ||       // same item type?
			(itrS->m_Item.m_ItemCount > a_CraftingGrid[GridID].m_ItemCount) ||  // not enough items
			(
				(itrS->m_Item.m_ItemHealth > 0) &&  // should compare damage values?
				(itrS->m_Item.m_ItemHealth != a_CraftingGrid[GridID].m_ItemHealth)
			)
		)
		{
			// Doesn't match
			return NULL;
		}
		HasMatched[itrS->x + a_OffsetX][itrS->y + a_OffsetY] = true;
	}  // for itrS - Recipe->m_Ingredients[]
	
	// Process the "Anywhere" items now, and only in the cells that haven't matched yet
	// The "anywhere" items are processed on a first-come-first-served basis.
	// Do not use a recipe with one horizontal and one vertical "anywhere" ("*:1, 1:*") as it may not match properly!
	cRecipeSlots MatchedSlots;  // Stores the slots of "anywhere" items that have matched, with the match coords
	for (cRecipeSlots::const_iterator itrS = a_Recipe->m_Ingredients.begin(); itrS != a_Recipe->m_Ingredients.end(); ++itrS)
	{
		if ((itrS->x >= 0) && (itrS->y >= 0))
		{
			// Regular item, already processed
			continue;
		}
		int StartX = 0, EndX = a_GridWidth  - 1;
		int StartY = 0, EndY = a_GridHeight - 1;
		if (itrS->x >= 0)
		{
			StartX = itrS->x;
			EndX = itrS->x;
		}
		else if (itrS->y >= 0)
		{
			StartY = itrS->y;
			EndY = itrS->y;
		}
		bool Found = false;
		for (int x = StartX; x <= EndX; x++) for (int y = StartY; y <= EndY; y++)
		{
			if (HasMatched[x][y])
			{
				// Already matched some other item
				continue;
			}
			int GridIdx = x + a_GridStride * y;
			if (
				(a_CraftingGrid[GridIdx].m_ItemID == itrS->m_Item.m_ItemID) &&
				(
					(itrS->m_Item.m_ItemHealth < 0) ||  // doesn't want damage comparison
					(itrS->m_Item.m_ItemHealth == a_CraftingGrid[GridIdx].m_ItemHealth)  // the damage matches
				)
			)
			{
				HasMatched[x][y] = true;
				Found = true;
				MatchedSlots.push_back(*itrS);
				MatchedSlots.back().x = x;
				MatchedSlots.back().y = y;
				break;
			}
		}  // for y, for x - "anywhere"
		if (!Found)
		{
			return NULL;
		}
	}  // for itrS - a_Recipe->m_Ingredients[]
	
	// Check if the whole grid has matched:
	for (int x = 0; x < a_GridWidth; x++) for (int y = 0; y < a_GridHeight; y++)
	{
		if (!HasMatched[x][y] && !a_CraftingGrid[x + a_GridStride * y].IsEmpty())
		{
			// There's an unmatched item in the grid
			return NULL;
		}
	}  // for y, for x
	
	// The recipe has matched. Create a copy of the recipe and set its coords to match the crafting grid:
	std::auto_ptr<cRecipe> Recipe(new cRecipe);
	Recipe->m_Result = a_Recipe->m_Result;
	Recipe->m_Width  = a_Recipe->m_Width;
	Recipe->m_Height = a_Recipe->m_Height;
	for (cRecipeSlots::const_iterator itrS = a_Recipe->m_Ingredients.begin(); itrS != a_Recipe->m_Ingredients.end(); ++itrS)
	{
		if ((itrS->x < 0) || (itrS->y < 0))
		{
			// "Anywhere" item, process later
			continue;
		}
		Recipe->m_Ingredients.push_back(*itrS);
	}
	Recipe->m_Ingredients.insert(Recipe->m_Ingredients.end(), MatchedSlots.begin(), MatchedSlots.end());
	return Recipe.release();
}



