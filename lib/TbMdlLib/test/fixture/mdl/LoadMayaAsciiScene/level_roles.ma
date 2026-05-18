//Maya ASCII 2020 scene
requires maya "2020";
createNode locator -n "tb_trigger_once";
	setAttr ".t" -type "double3" 32 0 32 ;
createNode transform -n "tb_spawn_info_player_deathmatch";
	setAttr ".t" -type "double3" -64 0 0 ;
createNode transform -n "tb_brush_func_detail";
	setAttr ".t" -type "double3" 0 0 0 ;
	setAttr ".s" -type "double3" 100 100 100 ;
createNode mesh -n "roomCubeShape" -p "tb_brush_func_detail";
	setAttr ".vt[0:7]" -type "float3"  -0.5 -0.5 -0.5
	 0.5 -0.5 -0.5
	 0.5 0.5 -0.5
	 -0.5 0.5 -0.5
	 -0.5 -0.5 0.5
	 0.5 -0.5 0.5
	 0.5 0.5 0.5
	 -0.5 0.5 0.5 ;
