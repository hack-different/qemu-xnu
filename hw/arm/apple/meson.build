if not tasn1.found()
    error('CONFIG_APPLE_SOC set but no libasn1 found')
endif
arm_ss.add(tasn1)

if not liblzfse.found()
    error('CONFIG_APPLE_SOC set but no liblzfse found')
endif
arm_ss.add(liblzfse)

arm_ss.add(files(
    's8000.c',
    't8030.c',
    't8030_cpu.c',
    't8030_gxf.c',
    'apple_a9.c',
    'apple_sep.c',
    'xnu_dtb.c',
    'xnu_mem.c',
    'xnu.c',
    'xnu_pf.c',
    'xnu_kpf.c')
)

arm_ss.add(when: 'CONFIG_APPLE_DART', if_true: files('apple_dart.c'))
arm_ss.add(when: 'CONFIG_APPLE_SART', if_true: files('apple_sart.c'))
