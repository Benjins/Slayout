
#include "slayout.cpp"

int main(int argc, char** argv){
	const char* fileName = "test.slt";

	if (argc > 1) {
		fileName = argv[1];
	}

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
			BNS_VEC_FOREACH(ctx.nodes) {
				if (ptr->IsLayoutRect()) {
					printf("Rect %.*s:\n", BNS_LEN_START(ptr->name));
					printf("\tx: %f:\n", ptr->AsLayoutRect().x.AsLayoutValueSimple().val);
					printf("\ty: %f:\n", ptr->AsLayoutRect().y.AsLayoutValueSimple().val);
					printf("\tw: %f:\n", ptr->AsLayoutRect().width.AsLayoutValueSimple().val);
					printf("\th: %f:\n", ptr->AsLayoutRect().height.AsLayoutValueSimple().val);
				}
				else if (ptr->IsLayoutPage()) {
					printf("Page %.*s:\n", BNS_LEN_START(ptr->name));
					printf("\tx: %f:\n", ptr->AsLayoutPage().x.AsLayoutValueSimple().val);
					printf("\ty: %f:\n", ptr->AsLayoutPage().y.AsLayoutValueSimple().val);
					printf("\tw: %f:\n", ptr->AsLayoutPage().width.AsLayoutValueSimple().val);
					printf("\th: %f:\n", ptr->AsLayoutPage().height.AsLayoutValueSimple().val);
				}
				else if (ptr->IsLayoutPoint()) {
					printf("Point %.*s:\n", BNS_LEN_START(ptr->name));
					printf("\tx: %f:\n", ptr->AsLayoutPoint().x.AsLayoutValueSimple().val);
					printf("\ty: %f:\n", ptr->AsLayoutPoint().y.AsLayoutValueSimple().val);
				}
			}

			const char* fileName = "output/slayout.bmp";
			printf("Writeing out result to '%s'\n", fileName);
			RenderLayoutToBMPFile(&ctx, fileName);
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