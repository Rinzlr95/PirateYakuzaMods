# Pirate Yakuza gameplay mod investigation

Targets: 2x flotsam, 1.5x roaming enemy ship spawns, 3x money rewards.

## Download

[Download Flotsam2x Experimental ZIP](downloads/Flotsam2x-Experimental.zip)

On the ZIP page, click **Download raw file** to download it.
See [installation instructions](INSTALL-FLOTSAM.txt). This is an untested
flotsam-only candidate; money and enemy-ship changes are not included.

## Available candidate

Flotsam2x-Experimental changes 17 fields in two loose English database tables.
See INSTALL-FLOTSAM.txt for installation, limitations, test procedure and removal.
The game has not been run here. Enemy ships and money are not included yet.

Build with Python 3 (no packages required):

```
python tools/build_flotsam.py /path/to/db.spr.en.par /path/to/Flotsam2x-Experimental.zip
```

The builder rejects archives whose SHA-256 differs from the inspected upload.
It preserves original table lengths and all bytes outside the manifest's fields.
Do not add source game archives or extracted database dumps to version control.

## Evidence and remaining investigation

- All 1,424 PAR entries were extracted and checked against declared sizes.
- Both flotsam tables use Dragon Engine ARMP v2, structured storage.
- An independent reARMP reader confirmed all 17 baseline values.
- Build verification checks expected original values, edited byte ranges,
  output ZIP integrity, and the exact stored table payloads.
- Visible in-game density, timing ownership, SRMM loading, and performance
  still require a Windows game test.
- `reward_table_items.bin` names the money and money_end fields at structure
  offsets 8 and 52. These correspond to unnamed nested reward_table columns
  4 and 19. Only six reward_table rows have positive amounts there, so changing
  them would not establish a universal 3x money reward modifier.
- Other money sources include pirate_treasure_list.money, coliseum base_money,
  and character_npc_soldier_reward_item.money. Costs also use 'money' fields;
  blindly multiplying all matching names would change prices.
- Ship status reward fields are small IDs associated with reward tables.
  They must not be treated as direct cash amounts.
- No confirmed roaming enemy-ship population setting was found in this archive.
  Next small candidates to inspect: lua.par and
  stage_ngen/st_pirate_hawaii_sea.par. The larger entity_spr.par may be relevant
  later, but is not necessary to upload at this stage.

## Format references

- https://github.com/Kaplas80/ParManager (PAR and SLLZ v2)
- https://github.com/Ret-HZ/LibARMP (ARMP structures)
- https://github.com/Ret-HZ/reARMP (independent read-only verification)
- https://github.com/TheTrueColonel/ShinRyuModManager-CE
  (loose-file mod paths mirror data; extracted PAR directories omit .par)

The repository includes the builder, patch manifest, and packaged test ZIP.
