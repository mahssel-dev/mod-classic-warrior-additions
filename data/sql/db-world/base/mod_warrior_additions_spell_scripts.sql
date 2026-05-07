-- mod_warrior_additions: bind Thunderclap spell IDs to C++ script names.
-- Apply once to the world database. Safe to re-run (DELETE then INSERT).

DELETE FROM spell_script_names WHERE ScriptName IN
    ('spell_warrior_thunderclap_rend_spread', 'spell_warrior_thunderclap_ap_scaling');

-- Thunderclap ranks: 1-7 (classic–wrath), plus PvP/heroic variants
INSERT INTO spell_script_names (spell_id, ScriptName) VALUES
(6343,  'spell_warrior_thunderclap_rend_spread'),
(8198,  'spell_warrior_thunderclap_rend_spread'),
(8204,  'spell_warrior_thunderclap_rend_spread'),
(8205,  'spell_warrior_thunderclap_rend_spread'),
(11580, 'spell_warrior_thunderclap_rend_spread'),
(11581, 'spell_warrior_thunderclap_rend_spread'),
(25264, 'spell_warrior_thunderclap_rend_spread'),
(47501, 'spell_warrior_thunderclap_rend_spread'),
(47502, 'spell_warrior_thunderclap_rend_spread'),
(6343,  'spell_warrior_thunderclap_ap_scaling'),
(8198,  'spell_warrior_thunderclap_ap_scaling'),
(8204,  'spell_warrior_thunderclap_ap_scaling'),
(8205,  'spell_warrior_thunderclap_ap_scaling'),
(11580, 'spell_warrior_thunderclap_ap_scaling'),
(11581, 'spell_warrior_thunderclap_ap_scaling'),
(25264, 'spell_warrior_thunderclap_ap_scaling'),
(47501, 'spell_warrior_thunderclap_ap_scaling'),
(47502, 'spell_warrior_thunderclap_ap_scaling');
