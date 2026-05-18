//Maya ASCII 2020 scene
requires maya "2020";
createNode transform -n "tb_brush_hull_func_static";
	setAttr ".t" -type "double3" 0 0 0 ;
	setAttr ".s" -type "double3" 50 50 50 ;
createNode mesh -n "hullCubeShape" -p "tb_brush_hull_func_static";
	setAttr ".vt[0:7]" -type "float3"  -0.5 -0.5 -0.5
	 0.5 -0.5 -0.5
	 0.5 0.5 -0.5
	 -0.5 0.5 -0.5
	 -0.5 -0.5 0.5
	 0.5 -0.5 0.5
	 0.5 0.5 0.5
	 -0.5 0.5 0.5 ;
createNode transform -n "tb_brush_box_func_wall";
	setAttr ".t" -type "double3" 128 0 0 ;
	setAttr ".s" -type "double3" 40 40 40 ;
createNode mesh -n "boxCubeShape" -p "tb_brush_box_func_wall";
	setAttr ".vt[0:7]" -type "float3"  -0.5 -0.5 -0.5
	 0.5 -0.5 -0.5
	 0.5 0.5 -0.5
	 -0.5 0.5 -0.5
	 -0.5 -0.5 0.5
	 0.5 -0.5 0.5
	 0.5 0.5 0.5
	 -0.5 0.5 0.5 ;
