QoL Enhancements v1.0.0

The missing qualify of life update is here! This is a suite of small improvements to ease frustration and make gameplay a little smoother.

This mod is broken up into a core module and seven feature modules enabling different categories of features. CORE IS REQUIRED, the mod will not work without it. Other submodules can be installed on their own or in any combination depending on which features you want.


## Installation

CORE IS REQUIRED. Install `QoL Enhancements - Core (REQUIRED)` to your mods folder, along with any of the other modules you want.

If you downloaded the Optional Files zip, it includes some extra customization options. The Shopping Card folder contains color variants - drag the enclosed `QoL Enhancements - Shopping Card` folder into your mods folder, replacing the default Shopping Card module if you already installed it. The included `My Custom QoL Configuration` can be added to your mods folder to customize settings. It's recommended to use this approach instead of editing the config files included in the mods, as they can be overrided when updating.


## Feature Modules

### Pockets

- Stack splitting: Grab 5 and Grab 10.
- Trigger controls: ZL to drop items, ZR to use items (hold tools, wear clothing, or consume food)
- Hold to select: in multi select menus (storage sheds, Nook's Cranny), hold A and move the cursor to select multiple items
- Press Y to move items to storage while in your house
- Bypass some menus:
  - Press A while in home storage to move an item to your pockets
  - Press A to open presents, packages, and messages in bottles
  - Press A to claim items from the recycle box

### Controls

- Hold Y to quickly advance dialogue until you reach a menu or the end of conversation
  - Can also quickly dismiss catch messages for bugs and fish
  - Doesn't yet speed up animations, only text
- Hold Y to enter hoover mode
  - If you pick up an item, keep holding Y to continue picking up items
  - If you harvest a crop, keep holding Y to continue harvesting crops
  - If you pick weeds, keep holding Y to continue picking weeds
- Automatically use traveral tools
  - Try to walk into a river and you'll automatically vault over it
  - Try walking into a cliff or off a cliff and you'll automatically climb it
  - Auto equips the tool for you and re-equips the tool you were holding once you've finished traversal
- Swim with just the joystick, no need to press A
  - But if you do press A, it gives you a little boost
  - Hold it down to get the boost every couple seconds
- Enables tilting the camera while swimming

### Crafting

- Faster crafting animation (optional/configurable)
- Wraparound for quantity selection
  - You can press down once to go from crafting 1 to crafting 10
  - or press up once to go from crafting 10 to crafting 1
- Press Y to go to quantity selection from the item screen, bypassing the confirmation prompt
- All of these work with the cooking menus too
- Adds a 3D preview of items to the DIY Recipes app on your Nook Phone

### Faster Dialogue

- Speed up base dialogue and boosted dialogue (when pressing B or R)
- Dialogue and self talk (like catching fish/bugs) are automatically boosted
- Custom settings available to automatically boost dialogue from shop or service NPCs

### Catalog

- Prefixes item names with a star (★) for:
  - items that haven't been catalogued
  - recipes that haven't been learned yet
  - bugs/fish that haven't been donated
  - art that hasn't been donated
- Items in Nook Shopping, Hotel catalog, and HHP Catalog have a star badge on the item icon if it hasn't been cataloged
- Hollow star (☆) for item variants that haven't been cataloged
- Hollow star for counterfeit art that hasn't been catalogued (filled stars for genuine art; can be disabled)

### Shopping Card

- Brings the shopping card from City Folk to New Horizons
- Lets you buy and sell with your savings account balance
- Works in Nook's Cranny and Able Sisters
- Tom Nook also accepts card for paying for storage upgrades
- If you don't have enough bells in your savings, it will try to use bells in your pockets as well
  - The amount of bells shown in the UI while in a shop or Nook Shopping reflects the total of all bells in your savings and your pockets
- More color schemes to choose from in the Optional Files

### Fitting Room

- Add clothes to your shopping bag to buy many items in one transaction
- Lets you press B to cancel the "Do you want to wear your items out?" message

### Core

- Required for other modules to work
- Patches the Amiibo scanner to give you 5 minutes to scan instead of 15 seconds


## Custom Configuration

It's recommended to download the Optional Files and drop the `My Custom QoL Configuration` folder into your mods folder to give you a template for setting up custom configuration.

Configuration changes are made by creating your own custom "mod" that includes `.custom.ini` files for each module you want to configure. This is to avoid configuration gettig overwritten when updating, as your custom folder will be preserved.

The mod looks for these `.custom.ini` files in the `romfs/qol-enhancements/config` folder.


### core.custom.ini

`amiibo_timeout_frames` (default: `9000`) About five minutes at 30 updates/second. Accepts 0..65535; 0 preserves the native timeout.

`log_versions` (default: `true`) Reports Core/module versions.

`log_settings` (default: `true`) Reports effective settings.

`log_debug` (default: `false`) Enables detailed diagnostics.


### catalog.custom.ini

`art_donation_stars` (default: `true`) Genuine undonated art gets a filled star; uncataloged fake art gets an outline star. `false` restores ordinary catalog-status stars for art.


### pockets.custom.ini

`bypass_menu_open_containers` (default: `true`) Opens a container directly when Open is its sole menu option.

`bypass_menu_claim_recycle_box` (default: `true`) Bypasses a sole Claim option in the recycling box.

`bypass_menu_withdraw_home_storage` (default: `true`) Enables direct home-storage withdrawal.


### faster-dialogue.custom.ini

`base_speed` (default: `2`) Ordinary nominal text speed multiplier; accepts `1..30`.

`boost_speed` (default: `4`) Held B/R and automatic-category multiplier; accepts `1..30`.

`boost_ui_messages` (default: `true`) Automatically uses boost speed for recognized UI messages.

`boost_unnamed_messages` (default: `true`) Automatically uses boost speed for recognized unnamed/self messages.

`boost_npc_list` (default: empty) Comma-separated NPC IDs or localized visible names to boost automatically; up to 64 entries.


### crafting.custom.ini

`diy_app_preview` (default: `true`) Enables 3D previews in the DIY Recipes app.

`workbench_preview_tilt` (default: `true`) Enables vertical right-stick tilt in workbench previews.

`crafting_quantity_wrap` (default: `true`) Wraps between quantity endpoints after releasing and pressing again.

`crafting_speed` (default: `6`) Maximum automatic crafting/cooking ramp multiplier; accepts `1..30`, with larger positive integers clamped to `30`.


### controls.custom.ini

`hold_to_advance_dialogue` (default: `true`) Hold Y for nominal 30× text and automatic page advancement.

`hold_to_hoover` (default: `true`) Hold Y to repeat the first successful pickup/harvest/weed/moss category.

`auto_vault` (default: `true`) Automatically vaults valid rivers.

`auto_ladder` (default: `true`) Automatically climbs with handheld/placed ladders and vines.

`auto_equip_traversal` (default: `true`) Holds a pocketed pole/ladder and restores the previous tool afterward.

`auto_swim` (default: `true`) Enables analog swimming and timed arm strokes.

`swim_stroke_boost` (default: `true`) Adds a small propulsion to swimming strokes when pressing or holding A.