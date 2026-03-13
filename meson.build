project('simple-db', 'c',
  version: '0.1',
  default_options: ['warning_level=3', 'c_std=c23'])

add_project_arguments('-Wpedantic', language: 'c')
if host_machine.system() == 'windows'
  add_project_arguments('-D_CRT_SECURE_NO_WARNINGS', language: 'c')
endif

inc = include_directories('include')

common_src = [
  'src/pager.c',
  'src/database.c',
  'src/btree.c',
  'src/statement.c',
  'src/schema.c',
  'src/os_portability.c'
]

db_exe = executable('db',
  sources: ['src/main.c'] + common_src,
  include_directories: inc,
  install: true
)

unit_tests_exe = executable('unit_tests',
  sources: ['tests/unit_tests.c'] + common_src,
  include_directories: inc
)

test('unit tests', unit_tests_exe)

# Golden tests
# Use the Python runner for cross-platform consistency.
python = import('python').find_installation('python3')
golden_tests_script = files('tests/golden_tests/run_golden_tests.py')

test('golden tests', python,
  args: [golden_tests_script, db_exe.full_path(), meson.current_source_dir() / 'tests' / 'golden_tests'],
  depends: db_exe
)
