from KeywordCore import *

def update_all_keyword():
	items = [
		('NP2LEX_ABAQUS', 'stlABAQUS.c', 'ABAQUS.inp', 0, parse_apdl_api_file),
		('NP2LEX_ACTIONSCRIPT', 'stlActionScript.c', 'ActionScript.as', 1, parse_actionscript_api_file),
		('NP2LEX_AHK', 'stlAutoHotkey.c', ['AutoHotkey_L.ahk', 'AutoHotkey_H.ahk'], 0, parse_autohotkey_api_file),
		('NP2LEX_APDL', 'stlAPDL.c', 'APDL.cdb', 0, parse_apdl_api_file),
		('NP2LEX_ASYMPTOTE', 'stlAsymptote.c', 'Asymptote.asy', 1, parse_asymptote_api_file),
		('NP2LEX_AVS', 'stlAviSynth.c', 'AviSynth.avs', 0, parse_avisynth_api_file),
		('NP2LEX_AWK', 'stlAwk.c', 'Awk.awk', 1, parse_awk_api_file),
		('NP2LEX_BATCH', 'stlBatch.c', 'Batch.bat', 0, parse_batch_api_file),
		('NP2LEX_CMAKE', 'stlCMake.c', 'CMake.cmake', 0, parse_cmake_api_file),
		('NP2LEX_CSHARP', 'stlCSharp.c', 'CSharp.cs', 1, parse_csharp_api_file),
		('NP2LEX_D', 'stlD.c', 'D.d', 1, parse_dlang_api_file),
		('NP2LEX_DART', 'stlDart.c', 'Dart.dart', 0, parse_dart_api_file),
		('NP2LEX_GN', 'stlGN.c', 'GN.gn', 0, parse_gn_api_file),
		('NP2LEX_GRAPHVIZ', 'stlGraphViz.c', 'GraphViz.dot', 0, parse_graphviz_api_file),
			('NP2LEX_BLOCKDIAG', 'stlBlockdiag.c', 'blockdiag.diag', 0, parse_graphviz_api_file),
			('NP2LEX_CSS', 'stlCSS.c', ['CSS.css'], 0, parse_css_api_file),
		('NP2LEX_GO', 'stlGO.c', 'Go.go', 0, parse_go_api_file),
		('NP2LEX_HAXE', 'stlHaxe.c', 'Haxe.hx', 1, parse_haxe_api_file),
		('NP2LEX_INNO', 'stlInno.c', 'InnoSetup.iss', 0, parse_inno_setup_api_file),
		('NP2LEX_JAM', 'stlJamfile.c', 'Jamfile.jam', 0, parse_jam_api_file),
		('NP2LEX_JAVA', 'stlJava.c', 'Java.java', 1, parse_java_api_file),
			('NP2LEX_GROOVY', 'stlGroovy.c', 'Groovy.groovy', 1, parse_groovy_api_file),
				('NP2LEX_GRADLE', 'stlGradle.c', 'Gradle.gradle', 1, parse_gradle_api_file),
			('NP2LEX_KOTLIN', 'stlKotlin.c', 'Kotlin.kt', 0, parse_kotlin_api_file),
		('NP2LEX_JAVASCRIPT', 'stlJavaScript.c', 'JavaScript.js', 1, parse_javascript_api_file),
			('NP2LEX_COFFEESCRIPT', 'stlCoffeeScript.c', 'CoffeeScript.coffee', 0, parse_coffeescript_api_file),
			('NP2LEX_TYPESCRIPT', 'stlTypeScript.c', 'TypeScript.ts', 1, parse_typescript_api_file),
		('NP2LEX_JULIA', 'stlJulia.c', 'Julia.jl', 0, parse_julia_api_file),
		('NP2LEX_LLVM', 'stlLLVM.c', 'LLVM.ll', 0, parse_llvm_api_file),
		('NP2LEX_LUA', 'stlLua.c', 'Lua.lua', 0, parse_lua_api_file),
		('NP2LEX_NSIS', 'stlNsis.c', 'NSIS.nsi', 0, parse_nsis_api_file),
		('NP2LEX_PYTHON', 'stlPython.c', 'Python.py', 0, parse_python_api_file),
		('NP2LEX_R', 'stlR.c', 'R.r', 0, parse_r_api_file),
		('NP2LEX_REBOL', 'stlRebol.c', ['Rebol.r', 'Red.red'], 1, parse_rebol_api_file),
		('NP2LEX_RUBY', 'stlRuby.c', 'Ruby.rb', 0, parse_ruby_api_file),
		('NP2LEX_RUST', 'stlRust.c', 'Rust.rs', 0, parse_rust_api_file),
		# TODO: SQL Dialect, https://github.com/zufuliu/notepad2/issues/31
		('NP2LEX_SQL', 'stlSQL.c', [
								'MySQL.sql',
								'Oracle.sql',
								'PostgreSQL.sql',
								'SQL.sql',
								'SQLite3.sql',
								'Transact-SQL.sql',
								], 0, parse_sql_api_files),
		('NP2LEX_SWIFT', 'stlSwift.c', 'Swift.swift', 0, parse_swift_api_file),
		('NP2LEX_VIM', 'stlVim.c', 'Vim.vim', 0, parse_vim_api_file),
		# https://github.com/WebAssembly/wabt/blob/main/src/lexer-keywords.txt
		('NP2LEX_WASM', 'stlWASM.c', 'wasm-lexer-keywords.txt', 0, parse_wasm_lexer_keywords),
	]

	numKeyword = 16
	for rid, output, path, count, parse in items:
		if isinstance(path, str):
			keywordList = parse('lang/' + path)
		else:
			pathList = ['lang/' + name for name in path]
			keywordList = parse(pathList)
		if keywordList:
			output = '../src/EditLexers/' + output
			UpdateKeywordFile(rid, output, keywordList, numKeyword - count)

	update_lexer_keyword_attr('../src/Styles.c')

update_all_keyword()
