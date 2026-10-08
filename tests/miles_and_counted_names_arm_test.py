"""Check counted recipe-name buffers through compiled pocket/storage/ground hooks."""
from pocket_recipe_arm_test import Recipes
from storage_ground_arm_test import Storage, GroundRecipes

count=0
for base in (0x7100000000,0x7200000000):
    for cls in (Recipes,Storage,GroundRecipes):
        q=cls(base)
        plural=q.name+0x2d0
        for kind in (0,1,2,3):
            for learned in (False,True):
                q.write(q.name+0x1f8,plural);q.write(q.name+0x200,128,4)
                q.u.mem_write(plural,'spooky fences\0'.encode('utf-16-le'))
                marked=not learned and kind<3
                q.check(737,learned,kind=kind,text='spooky fence',
                        expected='★ Spooky fence' if marked else 'spooky fence')
                expected='★ Spooky fences' if marked and kind in (1,2) else 'spooky fences'
                assert bytes(q.u.mem_read(plural,(len(expected)+1)*2))==(expected+'\0').encode('utf-16-le')
                count+=1
        # An invalid plural capacity must not overrun its buffer.
        q.write(q.name+0x200,127,4)
        q.u.mem_write(plural,'spooky fences\0'.encode('utf-16-le'))
        q.check(737,kind=2,text='spooky fence',expected='★ Spooky fence')
        assert bytes(q.u.mem_read(plural,28))=='spooky fences\0'.encode('utf-16-le');count+=1

print(f'PASS: {count} counted-name cases across pockets, storage and ground')
