#include <stdio.h>

#include "../CppUtils/disc_union.h"

#include "../CppUtils/assert.cpp"
#include "../CppUtils/strings.cpp"
#include "../CppUtils/sexpr.cpp"

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

struct LayoutTextNode {
	LayoutNode* node;
	SubString text;
};

struct LayoutImageNode {
	LayoutNode* node;
	SubString text;
};

struct LayoutAlignX{
	LayoutNode* ref;
	LayoutNode* item;
};

struct LayoutBeneath {
	LayoutNode* above;
	LayoutNode* below;
	LayoutValue spacing;
};

struct LayoutRow {
	LayoutNode* result;
	Vector<LayoutNode*> elements;
};

#define DISC_MAC(mac)    \
	mac(LayoutTextNode)  \
	mac(LayoutImageNode) \
	mac(LayoutAlignX)    \
	mac(LayoutBeneath)   \
	mac(LayoutRow)

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
			}
			else {
				printf("image node requires a name!");
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

LayoutNode* GetLayoutNodeByName(LayoutContext* ctx, const SubString& name) {
	BNS_VEC_FOREACH(ctx->nodes) {
		if (ptr->name == name) {
			return ptr;
		}
	}

	return nullptr;
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

void LayoutContextVerbsPass(const Vector<BNSexpr>& sexprs, LayoutContext* ctx) {
	BNS_VEC_FOREACH(sexprs) {
		BNSexpr args;
		LayoutNode node;
		if (MatchSexpr(ptr, "(text @{} @{...})", { &args })) {
			SubString name;
			ExtractorNameFromSexprs(&args, &name);
			LayoutNode* node = GetLayoutNodeByName(ctx, name);
			ASSERT(node != nullptr);
			LayoutTextNode txt;
			txt.node = node;
			LayoutVerb verb;
			verb = txt;
			ctx->verbs.PushBack(verb);
		}
		else if (MatchSexpr(ptr, "(image @{} @{...})", { &args })) {
			SubString name;
			ExtractorNameFromSexprs(&args, &name);
			LayoutNode* node = GetLayoutNodeByName(ctx, name);
			ASSERT(node != nullptr);
			LayoutImageNode img;
			img.node = node;
			LayoutVerb verb;
			verb = img;
			ctx->verbs.PushBack(verb);
		}
		else if (MatchSexpr(ptr, "(row @{} @{...})", { &args })) {
			SubString name;
			ExtractorNameFromSexprs(&args, &name);
			LayoutNode* node = GetLayoutNodeByName(ctx, name);
			ASSERT(node != nullptr);
			LayoutRow row;
			row.result = node;

			BNS_VEC_FOREACH(args.AsBNSexprParenList().children) {
				BNSexpr elems;
				if (MatchSexpr(ptr, "(elements @{id} @{...})", { &elems })) {
					BNS_VEC_FOREACH_NAME(elems.AsBNSexprParenList().children, elemPtr) {
						LayoutNode* elem = GetLayoutNodeByName(ctx, elemPtr->AsBNSexprIdentifier().identifier);
						ASSERT(elem != nullptr);
						row.elements.PushBack(elem);
					}
				}
			}

			LayoutVerb verb;
			verb = row;
			ctx->verbs.PushBack(verb);
		}
		else if (MatchSexpr(ptr, "(align-x @{} @{...})", { &args })) {
			BNSexpr ref, item;
			if (ExtractSubSexprByName(&args, "ref", &ref) && ExtractSubSexprByName(&args, "item", &item)) {
				if (ref.IsBNSexprIdentifier() && item.IsBNSexprIdentifier()) {
					LayoutAlignX alignX;
					alignX.ref  = GetLayoutNodeByName(ctx,  ref.AsBNSexprIdentifier().identifier);
					alignX.item = GetLayoutNodeByName(ctx, item.AsBNSexprIdentifier().identifier);
					LayoutVerb verb;
					verb = alignX;
					ctx->verbs.PushBack(verb);
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
					beneath.above = GetLayoutNodeByName(ctx, top.AsBNSexprIdentifier().identifier);
					beneath.below = GetLayoutNodeByName(ctx, bottom.AsBNSexprIdentifier().identifier);

					BNSexpr spacing;
					if (ExtractSubSexprByName(&args, "spacing", &spacing)) {
						ExtractLayoutValueFromSexpr(&spacing, &beneath.spacing);
					}
					else {
						beneath.spacing = LayoutValueSimple();
					}

					LayoutVerb verb;
					verb = beneath;
					ctx->verbs.PushBack(verb);
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
	}
}

enum LayoutSolverResult {
	LSR_Success,
	LSR_SomeProgress,
	LSR_NoProgress,
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
				return true;
				*outVal = ComputeOperator(op.AsBNSexprIdentifier().identifier, a1, a2);
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
	if (node->IsLayoutRect()) {
		for (int i = 0; i < 4; i++) {
			LayoutValue* val = &node->AsLayoutRect()[i];
			if (val->IsLayoutValueFormula()) {
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
			}
		}
	}
	else if (node->IsLayoutPoint()) {
		for (int i = 0; i < 2; i++) {
			LayoutValue* val = &node->AsLayoutPoint()[i];
			if (val->IsLayoutValueFormula()) {
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
			}
		}
	}
	else {
		ASSERT(false);
	}

	if (doMore) {
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
	float totalWidth = row->result->AsLayoutRect().width.AsLayoutValueSimple().val;
	float height = row->result->AsLayoutRect().height.AsLayoutValueSimple().val;
	float elemWidth = totalWidth / count;
	float y = row->result->AsLayoutRect().y.AsLayoutValueSimple().val;
	float currX = row->result->AsLayoutRect().x.AsLayoutValueSimple().val;

	BNS_VEC_FOREACH(row->elements) {
		(*ptr)->AsLayoutRect().width = LayoutValueSimple(elemWidth);
		(*ptr)->AsLayoutRect().y = LayoutValueSimple(y);
		(*ptr)->AsLayoutRect().x = LayoutValueSimple(currX);

		if ((*ptr)->AsLayoutRect().height.type == LayoutValue::UE_None) {
			(*ptr)->AsLayoutRect().height = LayoutValueSimple(height);
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
	else {
		ASSERT(false);
		return nullptr;
	}
}

LayoutSolverResult StepLayoutVerb(LayoutContext* ctx, LayoutVerb* verb) {
	if (verb->IsLayoutTextNode()) {
		LayoutNode* node = verb->AsLayoutTextNode().node;
		ASSERT(node->IsLayoutRect());
		return StepLayoutNode(node, ctx);
	}
	else if (verb->IsLayoutImageNode()) {
		LayoutNode* node = verb->AsLayoutImageNode().node;
		ASSERT(node->IsLayoutRect());
		return StepLayoutNode(node, ctx);
	}
	else if (verb->IsLayoutRow()) {
		LayoutNode* result = verb->AsLayoutRow().result;
		ASSERT(result->IsLayoutRect());
		LayoutSolverResult subRes = StepLayoutNode(result, ctx);
		if (subRes == LSR_Success) {
			StepLayoutRow(&verb->AsLayoutRow(), ctx);
		}

		return subRes;
	}
	else if (verb->IsLayoutAlignX()) {
		LayoutNode* ref  = verb->AsLayoutAlignX().ref;
		LayoutNode* item = verb->AsLayoutAlignX().item;

		ASSERT(item->IsLayoutRect());
		LayoutValue* xValue = GetLayoutValueX(ref);
		if (xValue->IsLayoutValueSimple()) {
			float val = xValue->AsLayoutValueSimple().val;
			item->AsLayoutRect().x = LayoutValueSimple(val);
			return LSR_Success;
		}
		else {
			return LSR_NoProgress;
		}
	}
	else if (verb->IsLayoutBeneath()) {
		LayoutNode* above = verb->AsLayoutBeneath().above;
		LayoutNode* below = verb->AsLayoutBeneath().below;

		ASSERT(above->IsLayoutRect());
		ASSERT(below->IsLayoutRect());

		if (above->AsLayoutRect().y.IsLayoutValueSimple()
		 && above->AsLayoutRect().height.IsLayoutValueSimple()) {
			float y = above->AsLayoutRect().y.AsLayoutValueSimple().val;
			float height = above->AsLayoutRect().height.AsLayoutValueSimple().val;
			float val = y - height;
			below->AsLayoutRect().y = LayoutValueSimple(y - height);
			return LSR_Success;
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
	return verb->IsLayoutTextNode() || verb->IsLayoutImageNode();
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
		else if (subRes == LSR_NoProgress) {
			needMore = true;
		}
		else {
			return LSR_Error;
		}
	}

	return needMore ? (progress ? LSR_SomeProgress : LSR_NoProgress) : LSR_Success;
}

int main(){
	const char* fileName = "test.slt";
	String fileContents = ReadStringFromFile(fileName);

	Vector<BNSexpr> sexprs;
	BNSexprParseResult res = ParseSexprs(&sexprs, fileContents);
	if (res != BNSexpr_Success) {
		printf("Error parsing file '%s'.", fileName);
	}
	else {
		LayoutContext ctx;
		InitLayoutContext(&ctx, 850, 1100);
		LayoutContextSkeletonPass(sexprs, &ctx);
		LayoutContextVerbsPass(sexprs, &ctx);

		LayoutSolverResult res = LSR_SomeProgress;
		while (res == LSR_SomeProgress) {
			printf("Stepping solver...\n");
			res = StepLayoutContextSolver(&ctx);
		}

		if (res == LSR_Success) {
			printf("Success!\n");
		}
		else if (res == LSR_NoProgress){
			printf("Solver could not make progress!\n");
		}
		else if (res == LSR_Error) {
			printf("Error encountered during solving!\n");
		}
	}
		
	return 0;
}