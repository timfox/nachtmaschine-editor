//Maya ASCII 2020 scene
requires maya "2020";
currentUnit -l centimeter -a degree;
createNode transform -n "tb_entity_info_player_start";
	setAttr ".t" -type "double3" 128 0 256 ;
createNode transform -n "tb_entity_light";
	setAttr ".t" -type "double3" 0 64 0 ;
	setAttr ".tb_class" -type "string" "light";
createNode transform -n "tb_model_crate";
	setAttr ".tb_model" -type "string" "models/props/crate.lwo";
createNode mesh -n "crateShape" -p "tb_model_crate";
