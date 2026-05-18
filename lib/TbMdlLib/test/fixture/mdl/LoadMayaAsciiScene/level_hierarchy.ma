//Maya ASCII 2020 scene
requires maya "2020";
createNode transform -n "level_root";
	setAttr ".t" -type "double3" 100 0 0 ;
createNode transform -n "tb_entity_info_player_start";
	setAttr ".t" -type "double3" 0 0 50 ;
parent "tb_entity_info_player_start" "level_root";
