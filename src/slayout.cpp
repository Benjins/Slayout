#include <stdio.h>

#include "../CppUtils/disc_union.h"

#include "../CppUtils/assert.cpp"
#include "../CppUtils/strings.cpp"
#include "../CppUtils/sexpr.cpp"
#include "../CppUtils/stringmap.cpp"
#include "../CppUtils/filesys.cpp"
#include "../CppUtils/hash.cpp"

struct LayoutValue;

struct LayoutValueFormula {
	BNSexpr sexpr;
};

struct LayoutValueSimple {
	float val;
	LayoutValueSimple(float _val = 0.0f) {
		val = _val;
	}
};

#define DISC_MAC(mac)       \
	mac(LayoutValueSimple)  \
	mac(LayoutValueFormula)

DEFINE_DISCRIMINATED_UNION(LayoutValue, DISC_MAC)

#undef DISC_MAC

struct LayoutRect {
	struct {
		LayoutValue x, y;
		LayoutValue width, height;
	};

	LayoutValue& operator[](int index) {
		ASSERT(index >= 0 && index < 4);
		return ((LayoutValue*)this)[index];
	}
};

static_assert(sizeof(LayoutRect) == 4 * sizeof(LayoutValue), "Check LayoutRect size");

struct LayoutPoint {
	struct {
		LayoutValue x, y;
	};

	LayoutValue& operator[](int index) {
		ASSERT(index >= 0 && index < 2);
		return ((LayoutValue*)this)[index];
	}
};

static_assert(sizeof(LayoutPoint) == 2 * sizeof(LayoutValue), "Check LayoutPoint size");

struct LayoutPage {
	LayoutValue x, y;
	LayoutValue width, height;
	LayoutValue marginTop, marginBottom, marginLeft, marginRight;
};

#define DISC_MAC(mac)   \
	mac(LayoutPoint)    \
	mac(LayoutRect)     \
	mac(LayoutPage)

DEFINE_DISCRIMINATED_UNION(LayoutNodeData, DISC_MAC)

#undef DISC_MAC

// TODO: inheritance, blurgh.
struct LayoutNode : LayoutNodeData {
	SubString name;
};

typedef int LayoutNodeIndex;

struct LayoutTextNode {
	LayoutNodeIndex node;
	BNSexpr text;
};

struct LayoutImageNode {
	LayoutNodeIndex node;
	SubString imgName;
};

struct LayoutRectNode {
	LayoutNodeIndex node;
};

struct LayoutAlignX{
	LayoutNodeIndex ref;
	LayoutNodeIndex item;
};

struct LayoutBeneath {
	LayoutNodeIndex above;
	LayoutNodeIndex below;
	LayoutValue spacing;
};

struct LayoutRow {
	LayoutNodeIndex result;
	Vector<LayoutNodeIndex> elements;
};

struct LayoutLine {
	LayoutNodeIndex from;
	LayoutNodeIndex to;
};

struct LayoutTextFill {
	LayoutNodeIndex tbox;
};

#define DISC_MAC(mac)    \
	mac(LayoutTextNode)  \
	mac(LayoutRectNode)  \
	mac(LayoutImageNode) \
	mac(LayoutAlignX)    \
	mac(LayoutBeneath)   \
	mac(LayoutRow)       \
	mac(LayoutLine)      \
	mac(LayoutTextFill)

DEFINE_DISCRIMINATED_UNION(LayoutVerb, DISC_MAC)

#undef DISC_MAC

struct LayoutContext {
	Vector<LayoutNode> nodes;
	Vector<LayoutVerb> verbs;
};

void InitLayoutContext(LayoutContext* ctx, float width, float height) {
	LayoutPage page;
	page.marginBottom = LayoutValueSimple(20.0f);
	page.marginTop = LayoutValueSimple(20.0f);
	page.marginLeft = LayoutValueSimple(20.0f);
	page.marginRight = LayoutValueSimple(20.0f);
	page.width = LayoutValueSimple(width);
	page.height = LayoutValueSimple(height);
	page.x = LayoutValueSimple(0);
	page.y = LayoutValueSimple(0);
	LayoutNode pageNode;
	pageNode.Assign(page);
	pageNode.name = STATIC_TO_SUBSTRING("page");

	ctx->nodes.PushBack(pageNode);
}

void ExtractLayoutValueFromSexpr(BNSexpr* sexpr, LayoutValue* val) {
	BNSexpr valExpr;
	if (MatchSexpr(sexpr, "@{num}", { &valExpr })) {
		*val = LayoutValueSimple(valExpr.AsBNSexprNumber().CoerceDouble());
	}
	else {
		LayoutValueFormula formula;
		formula.sexpr = *sexpr;
		*val = formula;
	}
}

void ExtractXYWH(BNSexpr* sexpr, LayoutRect* rect) {
	ASSERT(sexpr->IsBNSexprParenList());
	BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children) {
		BNSexpr val;
		if (MatchSexpr(ptr, "(x @{})", { &val })) {
			ExtractLayoutValueFromSexpr(&val, &rect->x);
		}
		else if (MatchSexpr(ptr, "(y @{})", { &val })) {
			ExtractLayoutValueFromSexpr(&val, &rect->y);
		}
		else if (MatchSexpr(ptr, "(width @{})", { &val })) {
			ExtractLayoutValueFromSexpr(&val, &rect->width);
		}
		else if (MatchSexpr(ptr, "(height @{})", { &val })) {
			ExtractLayoutValueFromSexpr(&val, &rect->height);
		}
	}
}

void ExtractXY(BNSexpr* sexpr, LayoutPoint* pt) {
	ASSERT(sexpr->IsBNSexprParenList());
	BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children) {
		BNSexpr val;
		if (MatchSexpr(ptr, "(x @{})", { &val })) {
			ExtractLayoutValueFromSexpr(&val, &pt->x);
		}
		else if (MatchSexpr(ptr, "(y @{})", { &val })) {
			ExtractLayoutValueFromSexpr(&val, &pt->y);
		}
	}
}

bool ExtractorNameFromSexprs(BNSexpr* sexprs, SubString* outName) {
	ASSERT(sexprs->IsBNSexprParenList());
	BNS_VEC_FOREACH(sexprs->AsBNSexprParenList().children) {
		BNSexpr val;
		if (MatchSexpr(ptr, "(name @{id})", { &val })) {
			*outName = val.AsBNSexprIdentifier().identifier;
			return true;
		}
	}

	return false;
}

bool ExtractSubSexprByName(BNSexpr* sexpr, const char* name, BNSexpr* outVal) {
	ASSERT(sexpr->IsBNSexprParenList());
	BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children) {
		if (MatchSexpr(ptr, StringStackBuffer<256>("(%s @{})", name).buffer, { outVal })) {
			return true;
		}
	}

	return false;
}

bool ExtractSubSexprsByName(BNSexpr* sexpr, const char* name, BNSexpr* outVal) {
	ASSERT(sexpr->IsBNSexprParenList());
	BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children) {
		if (MatchSexpr(ptr, StringStackBuffer<256>("(%s @{} @{...})", name).buffer, { outVal })) {
			return true;
		}
	}

	return false;
}

void LayoutContextSkeletonPass(const Vector<BNSexpr>& sexprs, LayoutContext* ctx) {
	BNS_VEC_FOREACH(sexprs) {
		BNSexpr args;
		LayoutNode node;
		if (MatchSexpr(ptr, "(text @{} @{...})", { &args })) {
			if (ExtractorNameFromSexprs(&args, &node.name)) {
				LayoutRect rect;
				ExtractXYWH(&args, &rect);
				node.Assign(rect);
				ctx->nodes.PushBack(node);

				LayoutTextNode txt;
				txt.node = ctx->nodes.count - 1;
				txt.text = *ptr;
				ctx->verbs.PushBack(txt);
			}
			else {
				printf("text node requires a name!");
				ASSERT(false);
			}
		}
		else if (MatchSexpr(ptr, "(image @{} @{...})", { &args })) {
			if (ExtractorNameFromSexprs(&args, &node.name)) {
				LayoutRect rect;
				ExtractXYWH(&args, &rect);
				node.Assign(rect);
				ctx->nodes.PushBack(node);

				LayoutImageNode img;
				img.node = ctx->nodes.count - 1;
				BNSexpr imgSrc;
				if (ExtractSubSexprByName(&args, "src", &imgSrc)) {
					if (imgSrc.IsBNSexprString()) {
						img.imgName = imgSrc.AsBNSexprString().value;
						ctx->verbs.PushBack(img);
					}
					else {
						printf("src attribute on image node needs to be a string.\n");
						ASSERT(false);
					}
				}
				else {
					printf("An image node needs to have its src specified.\n");
					ASSERT(false);
				}
			}
			else {
				printf("image node requires a name!");
				ASSERT(false);
			}
		}
		else if (MatchSexpr(ptr, "(line @{} @{...})", { &args })) {
			if (ExtractorNameFromSexprs(&args, &node.name)) {
				BNSexpr fromSexpr, toSexpr;
				bool isGood = ExtractSubSexprsByName(&args, "from", &fromSexpr);
				isGood &= ExtractSubSexprsByName(&args, "to", &toSexpr);
				ASSERT(isGood);

				LayoutPoint from;
				ExtractXY(&fromSexpr, &from);
				node.Assign(from);
				ctx->nodes.PushBack(node);

				LayoutPoint to;
				ExtractXY(&toSexpr, &to);
				node.Assign(to);
				ctx->nodes.PushBack(node);

				LayoutLine line;
				line.from = ctx->nodes.count - 2;
				line.to   = ctx->nodes.count - 1;
				ctx->verbs.PushBack(line);
			}
			else {
				printf("image node requires a name!");
				ASSERT(false);
			}
		}
		else if (MatchSexpr(ptr, "(rect @{} @{...})", { &args })) {
			if (ExtractorNameFromSexprs(&args, &node.name)) {
				LayoutRect rect;
				ExtractXYWH(&args, &rect);
				node.Assign(rect);
				ctx->nodes.PushBack(node);

				LayoutRectNode rectVerb;
				rectVerb.node = ctx->nodes.count - 1;
				ctx->verbs.PushBack(rectVerb);
			}
			else {
				printf("rect node requires a name!");
				ASSERT(false);
			}
		}
		else if (MatchSexpr(ptr, "(row @{} @{...})", { &args })) {
			if (ExtractorNameFromSexprs(&args, &node.name)) {
				LayoutRect rect;
				ExtractXYWH(&args, &rect);
				node.Assign(rect);
				ctx->nodes.PushBack(node);
			}
			else {
				printf("row node requires a name!");
				ASSERT(false);
			}
		}
	}
}

LayoutNodeIndex GetLayoutNodeIndexByName(LayoutContext* ctx, const SubString& name) {
	BNS_VEC_FOREACH(ctx->nodes) {
		if (ptr->name == name) {
			return ptr - ctx->nodes.data;
		}
	}

	return -1;
}

LayoutNode* GetLayoutNodeByName(LayoutContext* ctx, const SubString& name) {
	BNS_VEC_FOREACH(ctx->nodes) {
		if (ptr->name == name) {
			return ptr;
		}
	}

	return nullptr;
}

void LayoutContextVerbsPass(const Vector<BNSexpr>& sexprs, LayoutContext* ctx) {
	BNS_VEC_FOREACH(sexprs) {
		BNSexpr args;
		LayoutNode node;
		if (MatchSexpr(ptr, "(row @{} @{...})", { &args })) {
			SubString name;
			ExtractorNameFromSexprs(&args, &name);
			LayoutNodeIndex node = GetLayoutNodeIndexByName(ctx, name);
			ASSERT(node != -1);
			LayoutRow row;
			row.result = node;

			BNS_VEC_FOREACH(args.AsBNSexprParenList().children) {
				BNSexpr elems;
				if (MatchSexpr(ptr, "(elements @{id} @{...})", { &elems })) {
					BNS_VEC_FOREACH_NAME(elems.AsBNSexprParenList().children, elemPtr) {
						LayoutNodeIndex elem = GetLayoutNodeIndexByName(ctx, elemPtr->AsBNSexprIdentifier().identifier);
						ASSERT(elem != -1);
						row.elements.PushBack(elem);
					}
				}
			}

			ctx->verbs.PushBack(row);
		}
		else if (MatchSexpr(ptr, "(align-x @{} @{...})", { &args })) {
			BNSexpr ref, item;
			if (ExtractSubSexprByName(&args, "ref", &ref) && ExtractSubSexprByName(&args, "item", &item)) {
				if (ref.IsBNSexprIdentifier() && item.IsBNSexprIdentifier()) {
					LayoutAlignX alignX;
					alignX.ref  = GetLayoutNodeIndexByName(ctx,  ref.AsBNSexprIdentifier().identifier);
					alignX.item = GetLayoutNodeIndexByName(ctx, item.AsBNSexprIdentifier().identifier);
					ctx->verbs.PushBack(alignX);
				}
				else {
					printf("malformed align-x item");
					ASSERT(false);
				}
			}
			else {
				printf("malformed align-x item");
				ASSERT(false);
			}
		}
		else if (MatchSexpr(ptr, "(beneath @{} @{...})", { &args })) {
			BNSexpr top, bottom;
			if (ExtractSubSexprByName(&args, "top", &top) && ExtractSubSexprByName(&args, "bottom", &bottom)) {
				if (top.IsBNSexprIdentifier() && bottom.IsBNSexprIdentifier()) {
					LayoutBeneath beneath;
					beneath.above = GetLayoutNodeIndexByName(ctx, top.AsBNSexprIdentifier().identifier);
					beneath.below = GetLayoutNodeIndexByName(ctx, bottom.AsBNSexprIdentifier().identifier);

					BNSexpr spacing;
					if (ExtractSubSexprByName(&args, "spacing", &spacing)) {
						ExtractLayoutValueFromSexpr(&spacing, &beneath.spacing);
					}
					else {
						beneath.spacing = LayoutValueSimple(0);
					}

					ctx->verbs.PushBack(beneath);
				}
				else {
					printf("malformed align-x item");
					ASSERT(false);
				}
			}
			else {
				printf("malformed align-x item");
				ASSERT(false);
			}
		}
		else if (MatchSexpr(ptr, "(text-fill @{id})", { &args })) {
			LayoutTextFill fill;
			fill.tbox = GetLayoutNodeIndexByName(ctx, args.AsBNSexprIdentifier().identifier);
			ctx->verbs.PushBack(fill);
		}
	}
}

enum LayoutSolverResult {
	LSR_Success,
	LSR_SomeProgress,
	LSR_NoProgress,
	LSR_AlreadyDone,
	LSR_Error
};

float ComputeOperator(const SubString& opName, float a1, float a2) {
	     if (opName == "+") { return a1 + a2; }
	else if (opName == "*") { return a1 * a2; }
	else if (opName == "-") { return a1 - a2; }
	else if (opName == "/") { return a1 / a2; }
	else {
		ASSERT(false);
		return 0;
	}
}

bool GetPropertyOfNode(const SubString& propertyName, LayoutNode* node, float* outVal) {
#define GET_PROPERTY_IF_SIMPLE(prop, propName, type) else if (propertyName == propName ) { \
		if (node->As ## type(). prop .IsLayoutValueSimple()){ \
			*outVal = node->As ## type(). prop .AsLayoutValueSimple().val; \
			return true; \
		} \
		else{ \
			return false; \
		} \
	}

	if (node->IsLayoutPoint()) {
		// If false so we can have a chain of else-ifs afterward
		if (false) {}
		GET_PROPERTY_IF_SIMPLE(x, "x", LayoutPoint)
		GET_PROPERTY_IF_SIMPLE(y, "y", LayoutPoint)
		else { ASSERT(false); return false; }
	}
	else if (node->IsLayoutRect()) {
		if (false) {}
		GET_PROPERTY_IF_SIMPLE(x, "x", LayoutRect)
		GET_PROPERTY_IF_SIMPLE(y, "y", LayoutRect)
		GET_PROPERTY_IF_SIMPLE(width, "width", LayoutRect)
		GET_PROPERTY_IF_SIMPLE(height, "height", LayoutRect)
		else { ASSERT(false); return false; }
	}
	else if (node->IsLayoutPage()) {
		if (false) {}
		GET_PROPERTY_IF_SIMPLE(x, "x", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(y, "y", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(width, "width", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(height, "height", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(marginTop, "margin-top", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(marginBottom, "margin-bottom", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(marginLeft, "margin-left", LayoutPage)
		GET_PROPERTY_IF_SIMPLE(marginRight, "margin-right", LayoutPage)
		else { ASSERT(false); return false; }
	}
	else {
		ASSERT(false);
		return false;
	}
}

bool SolveFormula(BNSexpr* sexpr, LayoutContext* ctx, float* outVal) {
	if (sexpr->IsBNSexprNumber()) {
		*outVal = sexpr->AsBNSexprNumber().CoerceDouble();
		return true;
	}
	else {
		ASSERT(sexpr->IsBNSexprParenList());
		BNSexpr field, nodeName;
		BNSexpr op, arg1, arg2;
		if (MatchSexpr(sexpr, "(@{id} @{id})", { &field, &nodeName })) {
			LayoutNode* node = GetLayoutNodeByName(ctx, nodeName.AsBNSexprIdentifier().identifier);
			return GetPropertyOfNode(field.AsBNSexprIdentifier().identifier, node, outVal);
		}
		else if (MatchSexpr(sexpr, "(@{id} @{} @{})", {&op, &arg1, &arg2})) {
			float a1, a2;
			if (SolveFormula(&arg1, ctx, &a1) && SolveFormula(&arg2, ctx, &a2)) {
				*outVal = ComputeOperator(op.AsBNSexprIdentifier().identifier, a1, a2);
				return true;
			}
			else {
				return false;
			}
		}
		else {
			ASSERT(false);
			return false;
		}
	}
}

bool SolveFormula(LayoutValueFormula* formula, LayoutContext* ctx, float* outVal) {
	BNSexpr* sexpr = &formula->sexpr;
	return SolveFormula(sexpr, ctx, outVal);
}

LayoutSolverResult StepLayoutNode(LayoutNode* node, LayoutContext* ctx) {
	bool progress = false;
	bool doMore = false;
	bool alreadyDone = true;
	if (node->IsLayoutRect()) {
		for (int i = 0; i < 4; i++) {
			LayoutValue* val = &node->AsLayoutRect()[i];
			if (val->IsLayoutValueFormula()) {
				alreadyDone = false;
				float solvedVal;
				if (SolveFormula(&val->AsLayoutValueFormula(), ctx, &solvedVal)) {
					*val = LayoutValueSimple(solvedVal);
					progress = true;
				}
				else {
					doMore = true;
				}
			}
			else if (val->IsLayoutValueSimple()) {
				// Do nothing
			}
			else {
				doMore = true;
				alreadyDone = false;
			}
		}
	}
	else if (node->IsLayoutPoint()) {
		for (int i = 0; i < 2; i++) {
			LayoutValue* val = &node->AsLayoutPoint()[i];
			if (val->IsLayoutValueFormula()) {
				alreadyDone = false;
				float solvedVal;
				if (SolveFormula(&val->AsLayoutValueFormula(), ctx, &solvedVal)) {
					*val = LayoutValueSimple(solvedVal);
					progress = true;
				}
				else {
					doMore = true;
				}
			}
			else if (val->IsLayoutValueSimple()) {
				// Do nothing
			}
			else {
				doMore = true;
				alreadyDone = false;
			}
		}
	}
	else {
		ASSERT(false);
	}

	if (alreadyDone) {
		return LSR_AlreadyDone;
	}
	else if (doMore) {
		return progress ? LSR_SomeProgress : LSR_NoProgress;
	}
	else {
		return LSR_Success;
	}
}

bool LayoutNodeIsSolved(LayoutNode* node) {
	if (node->IsLayoutRect()) {
		for (int i = 0; i < 4; i++) {
			if (node->AsLayoutRect()[i].IsLayoutValueFormula()) {
				return false;
			}
		}

		return true;
	}
	else if (node->IsLayoutPoint ()) {
		for (int i = 0; i < 2; i++) {
			if (node->AsLayoutPoint()[i].IsLayoutValueFormula()) {
				return false;
			}
		}

		return true;
	}
	else {
		ASSERT(false);
		return false;
	}
}

void StepLayoutRow(LayoutRow* row, LayoutContext* ctx) {
	int count = row->elements.count;
	float totalWidth = ctx->nodes.data[row->result].AsLayoutRect().width.AsLayoutValueSimple().val;
	float height = ctx->nodes.data[row->result].AsLayoutRect().height.AsLayoutValueSimple().val;
	float elemWidth = totalWidth / count;
	float y = ctx->nodes.data[row->result].AsLayoutRect().y.AsLayoutValueSimple().val;
	float currX = ctx->nodes.data[row->result].AsLayoutRect().x.AsLayoutValueSimple().val;

	BNS_VEC_FOREACH(row->elements) {
		ctx->nodes.data[(*ptr)].AsLayoutRect().width = LayoutValueSimple(elemWidth);
		ctx->nodes.data[(*ptr)].AsLayoutRect().y = LayoutValueSimple(y);
		ctx->nodes.data[(*ptr)].AsLayoutRect().x = LayoutValueSimple(currX);

		if (ctx->nodes.data[(*ptr)].AsLayoutRect().height.type == LayoutValue::UE_None) {
			ctx->nodes.data[(*ptr)].AsLayoutRect().height = LayoutValueSimple(height);
		}

		currX += elemWidth;
	}
}

LayoutValue* GetLayoutValueX(LayoutNode* node) {
	if (node->IsLayoutRect()) {
		return &node->AsLayoutRect().x;
	}
	else if (node->IsLayoutPage()) {
		return &node->AsLayoutPage().x;
	}
	else if (node->IsLayoutPoint()) {
		return &node->AsLayoutPoint().x;
	}
	else {
		ASSERT(false);
		return nullptr;
	}
}

LayoutValue* GetLayoutValueWidth(LayoutNode* node) {
	if (node->IsLayoutRect()) {
		return &node->AsLayoutRect().width;
	}
	else if (node->IsLayoutPage()) {
		return &node->AsLayoutPage().width;
	}
	else {
		ASSERT(false);
		return nullptr;
	}
}

// TODO: There's got to be a better way... :p
LayoutSolverResult CombineSubResults(LayoutSolverResult res1, LayoutSolverResult res2) {
	if (res1 == LSR_Success && res2 == LSR_Success) {
		return LSR_Success;
	}
	else if (res1 == LSR_AlreadyDone && res2 == LSR_AlreadyDone) {
		return LSR_AlreadyDone;
	}
	else if (res1 == LSR_AlreadyDone && res2 == LSR_Success) {
		return LSR_Success;
	}
	else if (res1 == LSR_Success && res2 == LSR_AlreadyDone) {
		return LSR_Success;
	}
	else if (res1 == LSR_NoProgress && res2 == LSR_NoProgress) {
		return LSR_NoProgress;
	}
	else if (res1 == LSR_AlreadyDone && res2 == LSR_NoProgress) {
		return LSR_NoProgress;
	}
	else if (res1 == LSR_NoProgress && res2 == LSR_AlreadyDone) {
		return LSR_NoProgress;
	}
	else if (res1 == LSR_Error || res2 == LSR_Error) {
		return LSR_Error;
	}
	else {
		return LSR_SomeProgress;
	}
}

float GetHeightOfTextContentsSexpr(BNSexpr* sexpr, float x, float y, float w);

LayoutSolverResult StepLayoutVerb(LayoutContext* ctx, LayoutVerb* verb) {
	if (verb->IsLayoutTextNode()) {
		LayoutNode* node = &ctx->nodes.data[verb->AsLayoutTextNode().node];
		ASSERT(node->IsLayoutRect());
		return StepLayoutNode(node, ctx);
	}
	else if (verb->IsLayoutImageNode()) {
		LayoutNode* node = &ctx->nodes.data[verb->AsLayoutImageNode().node];
		ASSERT(node->IsLayoutRect());
		return StepLayoutNode(node, ctx);
	}
	else if (verb->IsLayoutRectNode()) {
		LayoutNode* node = &ctx->nodes.data[verb->AsLayoutRectNode().node];
		ASSERT(node->IsLayoutRect());
		return StepLayoutNode(node, ctx);
	}
	else if (verb->IsLayoutLine()) {
		LayoutNode* from = &ctx->nodes.data[verb->AsLayoutLine().from];
		LayoutNode* to   = &ctx->nodes.data[verb->AsLayoutLine().to];
		ASSERT(from->IsLayoutPoint());
		ASSERT(to->IsLayoutPoint());

		LayoutSolverResult subRes1 = StepLayoutNode(from, ctx);
		LayoutSolverResult subRes2 = StepLayoutNode(to, ctx);
		return CombineSubResults(subRes1, subRes2);
	}
	else if (verb->IsLayoutRow()) {
		LayoutNode* result = &ctx->nodes.data[verb->AsLayoutRow().result];
		ASSERT(result->IsLayoutRect());
		LayoutSolverResult subRes = StepLayoutNode(result, ctx);
		if (subRes == LSR_Success || subRes == LSR_AlreadyDone) {
			StepLayoutRow(&verb->AsLayoutRow(), ctx);
		}

		return subRes;
	}
	else if (verb->IsLayoutAlignX()) {
		LayoutNode* ref  = &ctx->nodes.data[verb->AsLayoutAlignX().ref];
		LayoutNode* item = &ctx->nodes.data[verb->AsLayoutAlignX().item];

		ASSERT(item->IsLayoutRect());
		LayoutValue* xValue = GetLayoutValueX(ref);
		LayoutValue* wValueRef  = GetLayoutValueWidth(ref);
		LayoutValue* wValueItem = GetLayoutValueWidth(item);
		if (xValue->IsLayoutValueSimple()
		 && wValueRef->IsLayoutValueSimple()
		 && wValueItem->IsLayoutValueSimple()) {
			float refX = xValue->AsLayoutValueSimple().val;
			float refW = wValueRef->AsLayoutValueSimple().val;
			float itemW = wValueItem->AsLayoutValueSimple().val;
			float val = refX + (refW / 2) - (itemW / 2);
			item->AsLayoutRect().x = LayoutValueSimple(val);
			return LSR_Success;
		}
		else {
			return LSR_NoProgress;
		}
	}
	else if (verb->IsLayoutBeneath()) {
		LayoutNode* above = &ctx->nodes.data[verb->AsLayoutBeneath().above];
		LayoutNode* below = &ctx->nodes.data[verb->AsLayoutBeneath().below];

		ASSERT(above->IsLayoutRect());
		ASSERT(below->IsLayoutRect());

		LayoutValue* spacing = &verb->AsLayoutBeneath().spacing;

		if (above->AsLayoutRect().y.IsLayoutValueSimple()
		 && above->AsLayoutRect().height.IsLayoutValueSimple()
		 && spacing->IsLayoutValueSimple()) {
			if (below->AsLayoutRect().y.IsLayoutValueSimple()) {
				return LSR_AlreadyDone;
			}
			else {
				float y = above->AsLayoutRect().y.AsLayoutValueSimple().val;
				float height = above->AsLayoutRect().height.AsLayoutValueSimple().val;
				float val = y - height - spacing->AsLayoutValueSimple().val;
				below->AsLayoutRect().y = LayoutValueSimple(val);
				return LSR_Success;
			}
		}
		else {
			return LSR_NoProgress;
		}
	}
	else if (verb->IsLayoutTextFill()) {
		LayoutNode* tbox = &ctx->nodes.data[verb->AsLayoutTextFill().tbox];
		ASSERT(tbox->IsLayoutRect());

		if (tbox->AsLayoutRect().x.IsLayoutValueSimple()
		 && tbox->AsLayoutRect().y.IsLayoutValueSimple()
		 && tbox->AsLayoutRect().width.IsLayoutValueSimple()) {

			if (tbox->AsLayoutRect().height.IsLayoutValueSimple()) {
				return LSR_AlreadyDone;
			}
			else {

				float x = tbox->AsLayoutRect().x.AsLayoutValueSimple().val;
				float y = tbox->AsLayoutRect().y.AsLayoutValueSimple().val;
				float w = tbox->AsLayoutRect().width.AsLayoutValueSimple().val;

				BNSexpr* textSexpr = nullptr;
				BNS_VEC_FOREACH_NAME(ctx->verbs, otherPtr) {
					if (otherPtr->IsLayoutTextNode()
						&& otherPtr->AsLayoutTextNode().node == verb->AsLayoutTextFill().tbox) {
						textSexpr = &otherPtr->AsLayoutTextNode().text;
						break;
					}
				}

				ASSERT(textSexpr != nullptr);

				float height = GetHeightOfTextContentsSexpr(textSexpr, x, y, w);
				tbox->AsLayoutRect().height = LayoutValueSimple(height);

				return LSR_Success;
			}
		}
		else {
			return LSR_NoProgress;
		}
	}
	else {
		ASSERT(false);
		return LSR_Error;
	}
}

inline bool IsVerbNeededForRendering(LayoutVerb* verb) {
	return verb->IsLayoutTextNode() || verb->IsLayoutImageNode() || verb->IsLayoutLine();
}

LayoutSolverResult StepLayoutContextSolver(LayoutContext* ctx) {
	bool progress = false;
	bool needMore = false;

	BNS_VEC_FOREACH(ctx->verbs) {
		LayoutSolverResult subRes = StepLayoutVerb(ctx, ptr);
		if (subRes == LSR_Success) {
			progress = true;
			// Ditch the verbs that are strictly for layout
			if (!IsVerbNeededForRendering(ptr)) {
				*ptr = ctx->verbs.Back();
				ctx->verbs.PopBack();
				ptr--;
			}
		}
		else if (subRes == LSR_SomeProgress) {
			progress = true;
			needMore = true;
		}
		else if (subRes == LSR_AlreadyDone) {
			//Do nothing
		}
		else if (subRes == LSR_NoProgress) {
			needMore = true;
		}
		else {
			ASSERT(false);
			return LSR_Error;
		}
	}

	return needMore ? (progress ? LSR_SomeProgress : LSR_NoProgress) : LSR_Success;
}

#include "slayout_gfx.cpp"
#include "slayout_text.cpp"

bool ParseColor(SubString colorText, int* outColVal) {
	ASSERT(outColVal != nullptr);
	if (colorText.length > 1 && colorText.start[0] == '#') {
		colorText.start++;
		colorText.length--;
		// TODO: Also length 3 colors
		if (colorText.length == 6) {
			// TODO: Validate we actually have a hex val
			*outColVal = HexAtoi(StringStackBuffer<16>("%.*s", BNS_LEN_START(colorText)).buffer);
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

SubString ChompQuotesOffString(SubString str) {
	ASSERT(str.length >= 2);
	ASSERT(str.start[0] == '"');
	ASSERT(str.start[str.length - 1] == '"');
	str.start++;
	str.length -= 2;

	return str;
}

LayoutTextJustification ParseJustification(const SubString& name) {
	if (name == "left") { return LTJ_Left; }
	else if (name == "right") { return LTJ_Right; }
	else if (name == "centre") { return LTJ_Centre; }
	else if (name == "full") { return LTJ_FullJustify; }
	else { ASSERT(false); return LTJ_Left; }
}

bool ParseTextContentsForTextEvents(BNSexpr* sexpr, Vector<LayoutTextEvent>* events) {
	BNSexpr rest;
	BNSexpr fontSize, fontName;
	BNSexpr col;
	BNSexpr justification;
	if (MatchSexpr(sexpr, "(text @{} @{...})", { &rest })) {
		BNSexpr unusedSexpr;
		BNSexpr* contentsSexpr;
		BNSexpr fontSize;
		int stylePushCount = 0;
		BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
			if (MatchSexpr(ptr, "(contents @{} @{...})", { &unusedSexpr })) {
				contentsSexpr = ptr;
			}
			else if (MatchSexpr(ptr, "(font-size @{num})", { &fontSize })) {
				events->PushBack(LayoutTextEventChangeScale(fontSize.AsBNSexprNumber().CoerceDouble()));
				stylePushCount++;
			}
		}

		if (contentsSexpr == nullptr) {
			ASSERT_WARN("%s", "Error: text node requires contents node.");
			return false;
		}
		else {
			bool isGood = ParseTextContentsForTextEvents(contentsSexpr, events);

			for (int i = 0; i < stylePushCount; i++) {
				events->PushBack(LayoutTextEventPopStyle());
			}

			return isGood;
		}
	}
	else if (MatchSexpr(sexpr, "(contents @{} @{...})", { &rest })) {
		bool isGood = true;
		BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
			isGood &= ParseTextContentsForTextEvents(ptr, events);
		}

		return isGood;
	}
	else if (MatchSexpr(sexpr, "(font @{str} @{num} @{} @{...})", { &fontName, &fontSize, &rest })) {
		events->PushBack(LayoutTextEventChangeFont(ChompQuotesOffString(fontName.AsBNSexprString().value)));
		events->PushBack(LayoutTextEventChangeScale(fontSize.AsBNSexprNumber().CoerceDouble()));

		bool isGood = true;
		BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
			isGood &= ParseTextContentsForTextEvents(ptr, events);
		}

		events->PushBack(LayoutTextEventPopStyle());
		events->PushBack(LayoutTextEventPopStyle());

		return isGood;
	}
	else if (MatchSexpr(sexpr, "(font @{str} @{} @{...})", { &fontName, &rest })) {
		events->PushBack(LayoutTextEventChangeFont(ChompQuotesOffString(fontName.AsBNSexprString().value)));

		bool isGood = true;
		BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
			isGood &= ParseTextContentsForTextEvents(ptr, events);
		}

		events->PushBack(LayoutTextEventPopStyle());

		return isGood;
	}
	else if (MatchSexpr(sexpr, "(font-size @{num} @{} @{...})", { &fontSize, &rest })) {
		events->PushBack(LayoutTextEventChangeScale(fontSize.AsBNSexprNumber().CoerceDouble()));

		bool isGood = true;
		BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
			isGood &= ParseTextContentsForTextEvents(ptr, events);
		}

		events->PushBack(LayoutTextEventPopStyle());

		return isGood;
	}
	else if (MatchSexpr(sexpr, "(justify @{id} @{} @{...})", { &justification, &rest })) {
		events->PushBack(LayoutTextEventChangeJustify(ParseJustification(justification.AsBNSexprIdentifier().identifier)));

		bool isGood = true;
		BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
			isGood &= ParseTextContentsForTextEvents(ptr, events);
		}

		events->PushBack(LayoutTextEventPopStyle());

		return isGood;
	}
	else if (MatchSexpr(sexpr, "(color @{id} @{} @{...})", { &col, &rest })) {
		int colVal;
		if (ParseColor(col.AsBNSexprIdentifier().identifier, &colVal)) {
			events->PushBack(LayoutTextEventChangeColor(colVal));

			bool isGood = true;
			BNS_VEC_FOREACH(rest.AsBNSexprParenList().children) {
				isGood &= ParseTextContentsForTextEvents(ptr, events);
			}

			events->PushBack(LayoutTextEventPopStyle());

			return isGood;
		}
		else {
			ASSERT(false);
			return false;
		}
	}
	else if (sexpr->IsBNSexprString()) {
		events->PushBack(LayoutTextEventDrawText(ChompQuotesOffString(sexpr->AsBNSexprString().value)));
		return true;
	}
	else {
		ASSERT(false);
		return false;
	}
}

float GetHeightOfTextContentsSexpr(BNSexpr* sexpr, float x, float y, float w) {
	Vector<LayoutTextEvent> events;
	ParseTextContentsForTextEvents(sexpr, &events);

	// TODO: Better way than this?
	// Not actually doing any render, but seems a bit sloppy
	LayoutRenderingContext rc;
	InitLayoutRenderingContext(&rc);

	float currX = x;
	float currY = y;

	Vector<LayoutTextEvent> newEvents;
	Vector<LayoutRenderTextLineInfo> lineInfo;
	LayoutTextEvents(events, &newEvents, x, w, &rc, &lineInfo);
	float sum = 0.0f;
	BNS_VEC_FOLDR(sum, lineInfo, 0.0f, acc + item.height);

	return sum;
}

void RenderLayoutToBMP(LayoutContext* ctx, BitmapData* bmp){
	LayoutNode* node = GetLayoutNodeByName(ctx, STATIC_TO_SUBSTRING("page"));
	
	bmp->width  = node->AsLayoutPage().width.AsLayoutValueSimple().val;
	bmp->height = node->AsLayoutPage().height.AsLayoutValueSimple().val;
	bmp->data = (int*)malloc(bmp->width * bmp->height * sizeof(int));

	LayoutRenderingContext rc;
	InitLayoutRenderingContext(&rc);

	BNS_VEC_FOREACH(ctx->verbs) {
		if (ptr->IsLayoutTextNode()) {
			LayoutNode* node = &ctx->nodes.data[ptr->AsLayoutTextNode().node];

			float x = node->AsLayoutRect().x.AsLayoutValueSimple().val;
			float y = node->AsLayoutRect().y.AsLayoutValueSimple().val;
			float w = node->AsLayoutRect().width.AsLayoutValueSimple().val;
			float h = node->AsLayoutRect().height.AsLayoutValueSimple().val;

			for (int i = 0; i < h; i++) {
				bmp->data[((int)y - i) * bmp->width + (int)x] = 0xFF4488FF;
				bmp->data[((int)y - i) * bmp->width + (int)x + (int)w] = 0xFF4488FF;
			}

			for (int i = 0; i < w; i++) {
				bmp->data[(int)y * bmp->width + (int)x + i] = 0xFF4488FF;
				bmp->data[((int)y - (int)h) * bmp->width + (int)x + i] = 0xFF4488FF;
			}

			Vector<LayoutTextEvent> events;
			bool good = ParseTextContentsForTextEvents(&ptr->AsLayoutTextNode().text, &events);
			if (good) {
				RenderTextEventsToBitmap(events, x, y, w, h, &rc, *bmp);
			}
			else {
				ASSERT(false);
			}
		}
		else if (ptr->IsLayoutImageNode()) {
			LayoutNode* node = &ctx->nodes.data[ptr->AsLayoutImageNode().node];
			int x = node->AsLayoutRect().x.AsLayoutValueSimple().val;
			int y = node->AsLayoutRect().y.AsLayoutValueSimple().val;
			int width = node->AsLayoutRect().width.AsLayoutValueSimple().val;
			int height = node->AsLayoutRect().height.AsLayoutValueSimple().val;

			SubString imgNameSubStr = ptr->AsLayoutImageNode().imgName;
			// Chomp off the quotes
			imgNameSubStr.start++;
			imgNameSubStr.length -= 2;
			StringStackBuffer<256> imgName("%.*s", BNS_LEN_START(imgNameSubStr));
			BitmapData imgData = rc.GetImage(imgName.buffer);
			if (imgData.data == nullptr) {
				printf("Error, could not open file '%s'", imgName.buffer);
				ASSERT(false);
			}
			else {
				BlitBitmap(*bmp, x, bmp->height - y, width, height, imgData);
			}
		}
		else if (ptr->IsLayoutLine()) {
			LayoutNode* from = &ctx->nodes.data[ptr->AsLayoutLine().from];
			LayoutNode* to   = &ctx->nodes.data[ptr->AsLayoutLine().to];
			int x0 = from->AsLayoutPoint().x.AsLayoutValueSimple().val;
			int y0 = from->AsLayoutPoint().y.AsLayoutValueSimple().val;
			int x1 = to->AsLayoutPoint().x.AsLayoutValueSimple().val;
			int y1 = to->AsLayoutPoint().y.AsLayoutValueSimple().val;

			DrawLine(*bmp, x0, bmp->height - y0, x1, bmp->height - y1, 3, 0xFF5577CC);
		}
	}
}

void RenderLayoutToBMPFile(LayoutContext* ctx, const char* fileName) {
	BitmapData data = {};
	RenderLayoutToBMP(ctx, &data);

	WriteBMPToFile(data, fileName);
	free(data.data);
}
