"""HHP chef reward card -> native ItemTarget 1 -> existing dialogue recipe star.

NNPC_RestaurantCook Event180 prepares the reward before Event179 names it.
The chef override in NpcSetRewardRecipe (00ac8e18..00ac8e94) obtains the daily
cooking recipe and calls 02512588. It stores that card at 05305f5c; ItemTarget 1
copies it unchanged. This differs from the villager's crafted-output target 16.
No live saves, reward generation or delivery side effects are exercised here.
"""
import struct
from dialogue_item_stars_arm_test import Dialogue, SITES, COOKING_IDS
from unicorn.arm64_const import *

count = 0
for base in (0x7100000000, 0x7200000000):
    q = Dialogue(base)
    q.select(SITES[-1])
    q.write(q.action + 0xd8, 1, 4)
    # The chef needs no current-craft NPC context: the reward carries its ID.
    q.write(q.event + 0x48, 0)
    for recipe in (COOKING_IDS[0], COOKING_IDS[len(COOKING_IDS)//2], COOKING_IDS[-1]):
        recipe_pointer = q.ram + 0x6000
        reward = recipe_pointer + 8
        q.write(recipe_pointer, recipe, 2)
        # Execute the native constructor's category validation and card-ID
        # initialization, excluding general ItemAccessor setup/metadata caches.
        q.u.reg_write(UC_ARM64_REG_SP, q.sp)
        q.u.reg_write(UC_ARM64_REG_X0, recipe_pointer)
        q.u.reg_write(UC_ARM64_REG_X8, reward)
        q.u.emu_start(base + 0x2512588, base + 0x25125e4)
        native_frame = q.u.reg_read(UC_ARM64_REG_X29)
        q.u.mem_write(reward, bytes(q.u.mem_read(native_frame-8, 8)))
        # Native cooking payload setter after ItemAccessor confirms recipe-card
        # kind. The real validity bits and payload store execute unchanged.
        q.u.reg_write(UC_ARM64_REG_X0, 5794)
        q.u.reg_write(UC_ARM64_REG_X19, reward-8)
        q.u.reg_write(UC_ARM64_REG_X20, recipe)
        q.u.emu_start(base + 0x1a05e5c, base + 0x1a05e98)
        card = bytes(q.u.mem_read(reward, 8))
        assert struct.unpack('<HBBI', card) == (5794, 0, 0, recipe), card
        # Native reward publication, followed by the actual target-1 copy.
        q.write(q.frame - 8, int.from_bytes(card, 'little'))
        q.u.reg_write(UC_ARM64_REG_X29, q.frame)
        q.u.emu_start(base + 0xac8ee4, base + 0xac8ef4)
        assert bytes(q.u.mem_read(base + 0x5305f5c, 8)) == card
        q.u.reg_write(UC_ARM64_REG_X19, q.source_item)
        q.u.emu_start(base + 0xa47078, base + 0xa47080)
        q.u.emu_start(base + 0xa47b14, base + 0xa47b34)
        assert bytes(q.u.mem_read(q.source_item, 8)) == card
        for player in range(8):
            q.mapped_player = 7-player
            for learned in (False, True):
                q.check_name(5794, '' if learned else '★', extra=recipe,
                             learned=learned, player=player)
                count += 1
print(f'PASS: {count} HHP chef recipe dialogue cases; native card encoding/reward/name copy and learned-state checks')
