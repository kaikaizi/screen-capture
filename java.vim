set noswf
let maplocalleader='q'
if !exists('b:PathRoot')	" get path root of current tabpage
   let path=getcwd()
   let pos=max([strridx(path,'/src/'), strridx(path,'/tst/')])
   if pos > 0
	let b:PathRoot=path[0:4+pos]
   end
end

inorea <buffer>print System.err.println();<Esc>hi
inorea <buffer>ret return
inorea <buffer>pub public
inorea <buffer>pri private
inorea <buffer>pro protected
inorea <buffer>fin final
inorea <buffer>cls class
inorea <buffer>boo boolean
inorea <buffer>Int Integer
inorea <buffer>Dou Double
inorea <buffer>str String
inorea <buffer>sta static
inorea <buffer>cat catch ()<Esc>i

inorea <buffer>pnotNull Preconditions.checkNotNull
inorea <buffer>anotNull Assert.assertNotNull
inorea <buffer>argument Preconditions.checkArgument

" add your commonly imports/static imports libs here
let s:imports = ["java.io.File", "java.io.FileInputStream", "java.io.FileNotFoundException",
         \"java.io.StringReader", "java.io.IOException", "java.nio.ByteBuffer",
         \"java.nio.charset.StandardCharsets",
         \
         \"java.util.Arrays", "java.util.ArrayList", "java.util.Collections", "java.util.Comparison",
         \"java.util.List", "java.util.HashMap", "java.util.regex.Pattern",
         \"java.util.Map", "java.util.ArrayList", "java.util.TreeMap", "java.util.StringBuilder",
         \
         \"javax.measure.unit.SI", "javax.measure.unit.Unit",
         \"org.joda.time.DateTime", "org.joda.time.format.DateTimeFormat", "org.joda.time.format.DateTimeFormatter",
         \"org.json.JSONObject", "org.json.JSONException",
         \
         \"org.slf4j.Logger", "org.slf4j.LoggerFactory",
         \
         \"org.springframework.beans.factory.annotation.Autowired", "org.springframework.beans.factory.annotation.Qualifier",
         \"org.springframework.stereotype.Component", "org.springframework.test.context.ContextConfiguration",
         \"org.springframework.test.context.junit4.SpringJUnit4ClassRunner",
         \
         \"com.google.common.base.Preconditions",
         \"org.apache.commons.lang.StringUtils", "org.apache.commons.lang.FileUtils", "org.apache.commons.io.IOUtils",
         \"org.apache.commons.cli.CommandLine", "org.apache.commons.cli.CommandLineParser",
         \"org.apache.commons.cli.HelpFormatter", "org.apache.commons.cli.Option", "org.apache.commons.cli.OptionBuilder",
         \"org.apache.commons.cli.Options", "org.apache.commons.cli.ParseException", "org.apache.commons.cli.PosixParser",
         \
         \"org.easymock.EasyMock", "org.easymock.IAnswer",
         \"org.powermock.api.mockito.PowerMockito", "org.powermock.core.classloader.annotations.PrepareForTest",
         \"org.powermock.modules.junit4.PowerMockRunner",
         \"org.powermock.api.easymock.PowerMock", "org.powermock.core.classloader.annotations.PowerMockIgnore",
         \"org.powermock.core.classloader.annotations.PrepareForTest",
         \
         \"org.junit.Test", "org.junit.After", "org.junit.Assert", "org.junit.Before", "org.junit.BeforeClass",
         \"org.junit.run.RunWith",
         \
         \"static org.hamcrest.MatcherAssert.assertThat", "static org.hamcrest.Matchers.equalTo",
         \"static org.hamcrest.Matchers.is", "static org.hamcrest.Matchers.nullValue",
         \
         \"java.util.concurrent.CompletableFuture", "java.util.concurrent.CompletionException",
         \"java.util.concurrent.ExecutorService", "java.util.concurrent.Executors", "java.util.concurrent.TimeUnit",
         \"java.util.concurrent.atomic.AtomicInteger",
         \
         \"com.fasterxml.jackson.annotation.JsonCreator", "com.fasterxml.jackson.annotation.JsonProperty",
         \"com.fasterxml.jackson.core.JsonProcessingException", "com.fasterxml.jackson.databind.ObjectMapper",
         \"com.fasterxml.jackson.databind.ObjectReader", "com.fasterxml.jackson.databind.ObjectWriter"]

fu! s:Complete(key)      " finds a matched import option
   for item in s:imports
      if len(matchstr(item, a:key))
         retu item
      end
   endfo
   retu ''
endf

fu! s:GetImportLineRange(target)
   let lineno=2         " skip first line
   let result=[0,0]     " end-by-line-number, match-line-number
   while lineno
      let line=getline(lineno)
      if !empty(line) && line !~ '^\(import \)\|\(package \)\|(\s*\/\/)'
         break
      elsei !empty(a:target) && len(matchstr(line, a:target))   " already exists
         let result[1]=lineno
      end
      let lineno += 1
   endw
   let result[0]=lineno-1
   retu result
endf

fu! s:Insert(imported)
   if empty(a:imported) | retu | end
   let imported='import '. a:imported . ';'
   let result=<SID>GetImportLineRange(imported)
   if result[1] | retu 0 | end      " already imported
   let lineno=result[0]
   " match not found: add it in appropriate place
   let lno=2 | let pos=lineno
   while lno<lineno
      if imported<getline(lno)   " insert before line #ln
         let pos=lineno
         break
      end
      let lno+=1
   endw
   cal append(pos, imported)
   retu 0
endf

fu! s:GetImportedLibs(lines)     " pre: lines do not contain empty lines
   let libs={}
   for line in a:lines
      let word=split(line, '\.')[-1]
      let libs[word[0: len(word)-2]]=1
   endfo
   retu keys(libs)
endf

fu! s:RemoveEntries(list, entries)  " pre: entries is a list of indices, has no duplicates; list is large enough for entries.
   let index=0
   cal reverse(sort(a:entries))
   while index<len(a:entries)
      cal remove(a:list, index)
   endfo
   retu a:list
endf

fu! s:RemoveImportedLib(lines, lib)
   let index=0
   while index<len(a:lines)
      let word=split(a:lines[index],'\.')[-1]
      let word=word[0 : len(word)-2]
      if word==a:lib
         cal remove(a:lines,index)
      else
         let index+=1
      end
   endw
endf

fu! s:GroupLines(lines)   " pre: lines is a list of import XX.YY.ZZ; post: in-place grouped, duplication removed.
   let index=1
   while index<len(a:lines)      " remove duplicated lines
      if a:lines[index]==a:lines[index-1]
         cal remove(a:lines, index)
      else
         let index+=1
      end
   endw
   " remove unused imports
   let src=split(substitute(join(getline(<SID>GetImportLineRange('')[0]+1, '$'), ' '),
            \'[<>()@.]', ' ', 'g'), '\s\+')
   for lib in <SID>GetImportedLibs(a:lines)
      if index(src,lib)<0
         cal <SID>RemoveImportedLib(a:lines, lib)
      end
   endfo
   " *do not* ignore commented regions/lines
   let newlines = []
   let prev_prefix=''
   for index in range(len(a:lines))    " group by first word
      let prefix=split(split(a:lines[index], '\s\+')[1], '\.')[0]
      if prefix!=prev_prefix
         let prev_prefix=prefix
         cal add(newlines,index)
      end
   endfo
   for index in reverse(newlines)
      cal insert(a:lines, '', index)
   endfo
   retu a:lines
endf

fu! s:SortImportLines()
   let line_range=<SID>GetImportLineRange('')
   if line_range[0] < 2 | retu 0 | end
   let prev_lines=getline(2,line_range[0])
   " first clean lines, then write back sorted version
   let lines=add(<SID>GroupLines(filter(sort(copy(prev_lines)), 'v:val !~ "^\\s*$"')), '')
   if prev_lines != lines
      exe '2,'. line_range[0] . 'd'
      call append(1, lines)
   end
endf

nmap <silent><buffer><LocalLeader>p :cal <SID>Insert(<SID>Complete(expand('<cword>')))<CR>
autocmd BufWritePre * :cal <SID>SortImportLines()
