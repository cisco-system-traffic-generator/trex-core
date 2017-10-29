#! /usr/bin/env python
# encoding: utf-8
# hhaim, 2014 (IL) base on WAF book

"""
call 'waf --targets=waf.pdf' or use 'waf list' to see the targets available
"""

VERSION='0.0.1'
APPNAME='wafdocs'

import os, re, shutil
import sys
import shlex
import subprocess
import json
import re
import tempfile
from waflib import Logs
import string

top = '.'
out = 'build'
sphinx_version = None

try:
    from HTMLParser import HTMLParser
except:
    from html.parser import HTMLParser

    
# HTML injector for TOC and DISQUS
from HTMLInjector import HTMLInjector


class CTocNode:
    def __init__ (self, link_fmt = None):
        self.name="root"
        self.level=1; # 1,2,3,4
        self.link=None;
        self.parent=None
        self.childs=[]; # link to CTocNode

        # by default, link_fmt will formatted as in-page links
        self.link_fmt = link_fmt if link_fmt else lambda x: '#' + x
        
    def get_link (self):
        if self.link==None:
            name=self.name
            l=name.split('.');
            l=l[-1].lower()
            s='';
            for c in l:
                if c.isalpha() or c.isspace():
                    s+=c
    
            link = '_'.join(s.lower().split());
        else:
            link = self.link
            
        return self.link_fmt(link)



    def add_new_child (self,name,level,link):
        n=CTocNode();
        n.name=name
        n.link=link
        n.level=level
        n.parent=self
        n.link_fmt=n.parent.link_fmt
        self.childs.append(n);
        return n

    def to_json_childs (self):
        l=[]
        for obj in self.childs:
            l.append(obj.to_json());
        return (l);

    def to_open (self):
        if self.level <3:
            return True
        else:
            return False


    def to_json (self):
        d={"text" : self.name,
           "link" : self.get_link(),
           "state"       : {
                "opened"    : self.to_open()
                }
          }
        if len(self.childs)>0 :
            d["children"]= self.to_json_childs()
        return d



class TocHTMLParser(HTMLParser):

    def __init__ (self, link_fmt = None):
        HTMLParser.__init__(self);
        self.state=0
        self.root = CTocNode(link_fmt)
        self.root.parent=self.root
        self.level=2;
        self.attrs=None
        self.d={};
        self.last_level=1
        self.set_level(1,self.root)


    def set_level (self,level,node):
        assert(node!=None);
        assert(isinstance(node,CTocNode)==True); 
        self.d[str(level)]=node

        # in case we change from high to low level remove the higher level 
        if level<self.last_level:
            for l in range(level+1,self.last_level+1):
                self.d.pop(str(l),None)



    def _get_level (self,level):
        k=str(level)
        if k in self.d:
            n=self.d[k]
            assert(n!=None);
            return n
        else:
            return None

    def get_level (self,level):
        for l in range(level,0,-1):
            n=self._get_level(l)
            if n != None:
                return n
        assert(0);


    def is_header (self,tag):
        if len(tag)==2 and tag[0]=='h' and tag[1].isdigit() and (int(tag[1])>1):
            return (True);

    def handle_starttag(self, tag, attrs):
        if self.is_header (tag):
            self.attrs=attrs
            self.state=True;
            self.level=int(tag[1]);

    def handle_endtag(self, tag):
        if self.is_header (tag):
            self.state=False;

    def get_id (self):
        if self.attrs:
            for obj in self.attrs:
                if obj[0]=='id':
                    return obj[1] 
        else:
            return None


    def handle_data(self, data):
        if self.state:
            
           level=self.level

           cnode=self.get_level(level-1)

           n=cnode.add_new_child(data,level,self.get_id());
           assert(n!=None);
           self.set_level(level,n) 
           self.last_level=level

    def dump_as_json (self):
        return json.dumps(self.root.to_json_childs(), sort_keys=False, indent=4)




def create_toc_json (input_file, output_file, link_fmt = None):
    f = open (input_file)
    l=f.readlines()
    f.close();
    html_input = ''.join(l)
    parser = TocHTMLParser(link_fmt)
    parser.feed(html_input);
    f = open (output_file,'w')
    f.write(parser.dump_as_json());
    f.close();




re_xi = re.compile('''^(include|image)::([^.]*.(asciidoc|\\{PIC\\}))\[''', re.M)
def ascii_doc_scan(self):
    p = self.inputs[0].parent
    node_lst = [self.inputs[0]]
    seen = []
    depnodes = []
    while node_lst:
        nd = node_lst.pop(0)
        if nd in seen: continue
        seen.append(nd)

        code = nd.read()
        for m in re_xi.finditer(code):
            name = m.group(2)
            if m.group(3) == '{PIC}':

                ext = '.eps'
                if self.generator.rule.rfind('A2X') > 0:
                    ext = '.png'

                k = p.find_resource(name.replace('{PIC}', ext))
                if k:
                    depnodes.append(k)
            else:
                k = p.find_resource(name)
                if k:
                    depnodes.append(k)
                    node_lst.append(k)
    return [depnodes, ()]
        
        

def scansize(self):
    name = 'image::%s\\{PIC\\}\\[.*,(width|height)=(\\d+)' % self.inputs[0].name[:-4]
    re_src = re.compile(name)
    lst = self.inputs[0].parent.get_src().ant_glob('*.txt')
    for x in lst:
        m = re_src.search(x.read())
        if m:
            val = str(int(1.6 * int(m.group(2))))
            if m.group(1) == 'width':
                w = val
                h = "800"
            else:
                w = "800"
                h = val

            ext = self.inputs[0].name[-3:]
            if ext == 'eps':
                code = '-geometry %sx%s' % (w, h)
            elif ext == 'dia':
                if m.group(1) == 'width':
                    h = ''
                else:
                    w = ''
                code = '--size %sx%s' % (w, h)
            else:
                code = '-Gsize="%s,%s"' % (w, h)
            break
    else:
        return ([], '')

    return ([], code)

def options(opt):
    opt.add_option('--exe', action='store_true', default=False, help='Execute the program after it is compiled')
    opt.add_option('--performance', action='store_true', help='Build a performance report based on google analytics')
    opt.add_option('--performance-detailed',action='store_true',help='print detailed test results (date,time, build id and results) to csv file named _detailed_table.csv.')
    opt.add_option('--ndr', action = 'store_true', help = 'Include build of NDR report.')

def configure(conf):
    search_path = ' ~/.local/bin /usr/local/bin/ /usr/bin ./extensions'
    conf.find_program('asciidoc', path_list=search_path, var='ASCIIDOC')
    conf.find_program('sphinx-build', path_list=search_path, var='SPHINX')
    conf.find_program('source-highlight', path_list=search_path, var='SRC_HIGHLIGHT')
    conf.find_program('dblatex', path_list=search_path, var='DBLATEX')
    conf.find_program('a2x', path_list=search_path, var='A2X')
    
    # asciidoctor
    conf.find_program('asciidoctor', path_list=search_path, var='ASCIIDOCTOR')
    conf.find_file('multipage-html5-converter.rb', path_list=search_path)
    

def convert_to_pdf(task):
    input_file = task.outputs[0].abspath()
    out_dir = task.outputs[0].parent.get_bld().abspath()
    return  os.system('a2x --no-xmllint %s -f pdf  -d  article %s -D %s ' %('-v' if Log.verbose else '', task.inputs[0].abspath(),out_dir ) )




def _convert_to_html_toc_book (task,disqus=False):
    input_file = task.inputs[0].abspath()

    json_out_file = os.path.splitext(task.outputs[0].abspath())[0]+'.json' 
    tmp = os.path.splitext(task.outputs[0].abspath())[0]+'.tmp' 
    json_out_file_short = os.path.splitext(task.outputs[0].name)[0]+'.json' 
    cmd='{0} -a stylesheet={1} -a  icons=true -a docinfo -d book  -o {2} {3}'.format(
            task.env['ASCIIDOC'][0],
            task.inputs[1].abspath(),
            tmp,
            task.inputs[0].abspath());

    res = os.system(cmd)
    if res != 0 :
        return (1)

    create_toc_json(tmp,json_out_file)

    in_file  = tmp
    out_file = task.outputs[0].abspath()
    
    # inject TOC and DISQUS if needed
    with HTMLInjector(in_file, out_file) as injector:
        injector.inject_toc(json_out_file_short)
        if disqus:
            injector.inject_disqus(os.path.split(out_file)[1], mode = 'plain')
    

    return os.system('rm {0}'.format(tmp));


    
def convert_to_html_toc_book(task):
    _convert_to_html_toc_book (task,False)

def convert_to_html_toc_book_disqus(task):
    _convert_to_html_toc_book (task,True)


# strip hierarchy from a multipage doc
# used to generate equal hierarchy for generating
# the pages
# also a 'main' hook will be added
def multipage_strip_hierarchy (in_file, out_file):
    
    with open(in_file) as f:
        lines = f.readlines()
    
    for i, line in enumerate(lines):
        if re.match("==+ [^ ].*", line):
            prefix, name = line.split(' ', 1)
            lines[i] = '== {0}\n'.format(name)
            
                
    # add a hook                
    lines.append('\n== main\n')
    
    with open(out_file, "w") as f:
        f.write(''.join(lines))
    
    
    
# generate an asciidoctor chunk book for a target
def convert_to_asciidoctor_chunk_book(task):

    in_file          = task.inputs[0].abspath()
    out_dir          = os.path.splitext(task.outputs[0].abspath())[0]
    target_name      = os.path.basename(out_dir)

    
    
    # build chunked with no hierarchy
    with tempfile.NamedTemporaryFile() as tmp_file:
        # strip the hierarchy to make sure all pages are generated
        multipage_strip_hierarchy(in_file, tmp_file.name)
    
        multipage_backend = os.path.join('./extensions', 'multipage-html5-converter.rb')
        cmd = '{0} -r {1} -b multipage_html5 -D {2} {3} -o trex_cookbook.html'.format(
                task.env['ASCIIDOCTOR'][0],
                multipage_backend,
                out_dir,
                tmp_file.name)

        res = os.system(cmd)
        if res != 0:
            return (1)
    
    
    # build single only to get the TOC
    with tempfile.NamedTemporaryFile() as tmp_file:
        cmd = '{0} -a toc=left -b html5 -o {2} {3}'.format(
                task.env['ASCIIDOCTOR'][0],
                multipage_backend,
                tmp_file.name,
                in_file)
    
        res = os.system( cmd )
        if res != 0:
            return (1)
        
        # create TOC JSON in the library
        create_toc_json(tmp_file.name, os.path.join(out_dir, 'toc.json'), link_fmt = lambda link : '{0}{1}.html'.format("trex_cookbook", link))
  
    
    # iterate over all files and inject DISQUS if the pattern matches
    for filename in [f for f in os.listdir(out_dir) if f.endswith('.html')]:
        with HTMLInjector(os.path.join(out_dir, filename)) as injector:
            injector.inject_disqus(page_id = filename, mode = 'pattern')
        
    
    # add TOC and disqus to the main page
    main = '{0}_main.html'.format(target_name)

    with HTMLInjector(os.path.join(out_dir, main),
                      os.path.join(out_dir, 'index.html')) as injector:
        injector.inject_toc('toc.json', fmt = 'multi', image_path = '..')

    os.remove(os.path.join(out_dir, main))

    return 0
        


def convert_to_chunked(task):
    return
    
    input_file = task.outputs[0].abspath()
    out_dir = task.outputs[0].parent.get_bld().abspath()
    cmd = 'a2x --xsl-file=/tmp/docbook-xsl-1.79.1/xhtml/chunk.xsl --no-xmllint %s -a icons=true -a docinfo -f chunked -d book %s -D %s ' %('-v' if Logs.verbose else '', task.inputs[0].abspath(),out_dir)
    print(cmd)
    return os.system(cmd)

    



def convert_to_pdf_book(task):
    input_file = task.outputs[0].abspath()
    out_dir = task.outputs[0].parent.get_bld().abspath()
    return os.system('a2x --no-xmllint %s -f pdf  -d book %s -D %s ' %('-v' if Logs.verbose else '', task.inputs[0].abspath(),out_dir ) )


def ensure_dir(f):
    if not os.path.exists(f):
        os.makedirs(f)
    

def my_copy(task):
    input_file=task.outputs[0].abspath()
    out_dir=task.outputs[0].parent.get_bld().abspath()
    ensure_dir(out_dir)
    shutil.copy2(input_file, out_dir+ os.sep+task.outputs[0].name)
    return (0)


def do_visio(bld):
    for x in bld.path.ant_glob('visio\\*.vsd'):
        tg = bld(rule='${VIS} -i ${SRC} -o ${TGT} ', source=x, target=x.change_ext('.png'))

def get_sphinx_version(sphinx_path):
    try:
        global sphinx_version
        if not sphinx_version:
            sphinx_version_regexp = '^Sphinx \(sphinx-build\) (\d+)\.(\d+)\.\d+$'
            cmd = '%s %s --version' % (sys.executable, sphinx_path)
            output = subprocess.check_output(shlex.split(cmd), universal_newlines = True)
            for line in output.splitlines():
                ver = re.match(sphinx_version_regexp, line)
                if ver:
                    sphinx_version = float('%s.%s' % (ver.group(1), ver.group(2)))
        return sphinx_version
    except Exception as e:
        print('Error getting Sphinx version: %s' % e)

def get_trex_core_git():
    trex_core_git_path = os.path.abspath(os.path.join(os.getcwd(), os.pardir))
    if not os.path.isdir(trex_core_git_path):
        trex_core_git_path = os.getenv('TREX_CORE_GIT', None)
    return trex_core_git_path

def parse_hlt_args(task):
    trex_core_git_path = get_trex_core_git()
    if not trex_core_git_path:
        return 1
    hltapi_path = os.path.abspath(os.path.join(trex_core_git_path, 'scripts', 'automation', 'trex_control_plane', 'stl', 'trex_stl_lib', 'trex_stl_hltapi.py'))
    header = ['[options="header",cols="<.^1,^.^1,9<.^e"]', '|=================', '^| Argument | Default ^| Comment']
    footer = ['|=================\n']
    hlt_asciidoc = []
    category_regexp = '^(\S+)_kwargs = {$'
    comment_line_regexp = '^\s*#\s*(.+)$'
    arg_line_regexp = "^\s*'([^']+)':\s*'?([^,']+)'?,\s*#?\s*(.+)?$"
    if not os.path.exists(hltapi_path):
        raise Exception('Could not find hltapi file: %s' % hltapi_path)
    with open(hltapi_path) as f:
        in_args = False
        for line in f.read().splitlines():
            if not in_args:
                if line.startswith('import'):
                    break
                category_line = re.match(category_regexp, line)
                if category_line:
                    hlt_asciidoc.append('\n===== %s\n' % category_line.group(1))
                    hlt_asciidoc += header
                    in_args = True
                continue
            comment_line = re.match(comment_line_regexp, line)
            if comment_line:
                hlt_asciidoc.append('3+^.^s| %s' % comment_line.group(1).replace('|', '\|'))
                continue
            arg_line = re.match(arg_line_regexp, line)
            if arg_line:
                arg, default, comment = arg_line.groups()
                hlt_asciidoc.append('| %s | %s | %s' % (arg, default, comment.replace('|', '\|') if comment else ''))
                continue
            if line == '}':
                hlt_asciidoc += footer
                in_args = False
    if not len(hlt_asciidoc):
        raise Exception('Parsing of hltapi args failed')
    with open('build/hlt_args.asciidoc', 'w') as f:
        f.write('\n'.join(hlt_asciidoc))
    return 0

def build_cp_docs (task):
    out_dir = task.outputs[0].abspath()
    export_path = os.path.join(os.getcwd(), 'build', 'cp_docs')
    trex_core_git_path = get_trex_core_git()
    if not trex_core_git_path: # there exists a default directory or the desired ENV variable.
        return 1
    trex_core_docs_path = os.path.abspath(os.path.join(trex_core_git_path, 'scripts', 'automation', 'trex_control_plane', 'doc'))
    sphinx_version = get_sphinx_version(task.env['SPHINX'][0])
    if not sphinx_version:
        return 1
    if sphinx_version < 1.3:
        additional_args = '-D html_theme=default'
    else:
        additional_args = ''
    build_doc_cmd = "{pyt} {sph} {add} {ver} -W -b {bld} {src} {dst}".format(
        pyt= sys.executable,
        sph= task.env['SPHINX'][0],
        add= additional_args,
        ver= '' if Logs.verbose else '-q',
        bld= "html", 
        src= ".",
        dst= out_dir)
    if Logs.verbose:
        print(build_doc_cmd)
    try:
        return subprocess.call(shlex.split(build_doc_cmd), cwd = trex_core_docs_path)
    except OSError as e:
        print('Failed command: %s\nError: %s' % (build_doc_cmd, e))
        return 1

def build_stl_cp_docs (task):
    out_dir = task.outputs[0].abspath()
    export_path = os.path.join(os.getcwd(), 'build', 'cp_stl_docs')
    trex_core_git_path = get_trex_core_git()
    if not trex_core_git_path: # there exists a default directory or the desired ENV variable.
        return 1
    trex_core_docs_path = os.path.abspath(os.path.join(trex_core_git_path, 'scripts', 'automation', 'trex_control_plane', 'doc_stl'))
    sphinx_version = get_sphinx_version(task.env['SPHINX'][0])
    if not sphinx_version:
        return 1
    if sphinx_version < 1.3:
        additional_args = '-D html_theme=default'
    else:
        additional_args = ''
    build_doc_cmd = "{pyt} {sph} {add} {ver} -W -b {bld} {src} {dst}".format(
        pyt= sys.executable,
        sph= task.env['SPHINX'][0],
        add= additional_args,
        ver= '' if Logs.verbose else '-q',
        bld= "html", 
        src= ".", 
        dst= out_dir)
    if Logs.verbose:
        print(build_doc_cmd)
    try:
        return subprocess.call(shlex.split(build_doc_cmd), cwd = trex_core_docs_path)
    except OSError as e:
        print('Failed command: %s\nError: %s' % (build_doc_cmd, e))
        return 1


def build_astf_cp_docs (task):
    out_dir = task.outputs[0].abspath()
    export_path = os.path.join(os.getcwd(), 'build', 'cp_astf_docs')
    trex_core_git_path = get_trex_core_git()
    if not trex_core_git_path: # there exists a default directory or the desired ENV variable.
        return 1
    trex_core_docs_path = os.path.abspath(os.path.join(trex_core_git_path, 'scripts', 'automation', 'trex_control_plane', 'doc_astf'))
    sphinx_version = get_sphinx_version(task.env['SPHINX'][0])
    if not sphinx_version:
        return 1
    if sphinx_version < 1.3:
        additional_args = '-D html_theme=default'
    else:
        additional_args = ''
    build_doc_cmd = "{pyt} {sph} {add} {ver} -W -b {bld} {src} {dst}".format(
        pyt= sys.executable,
        sph= task.env['SPHINX'][0],
        add= additional_args,
        ver= '' if Logs.verbose else '-q',
        bld= "html", 
        src= ".", 
        dst= out_dir)
    if Logs.verbose:
        print(build_doc_cmd)
    try:
        return subprocess.call(shlex.split(build_doc_cmd), cwd = trex_core_docs_path)
    except OSError as e:
        print('Failed command: %s\nError: %s' % (build_doc_cmd, e))
        return 1


def build_cp(bld,dir,root,callback):
    export_path = os.path.join(os.getcwd(), 'build', dir)
    trex_core_git_path = get_trex_core_git()
    if not trex_core_git_path: # there exists a default directory or the desired ENV variable.
        raise NameError("Environment variable 'TREX_CORE_GIT' is not defined.")
    trex_core_docs_path = os.path.join(trex_core_git_path, 'scripts', 'automation', 'trex_control_plane', root, 'index.rst')
    bld(rule=callback,target = dir)




def create_analytic_report(task):
    try:
        import AnalyticsWebReport as analytics
        if task.generator.bld.options.performance_detailed:
            analytics.main(verbose = Logs.verbose,detailed_test_stats='yes')
        else:
            analytics.main(verbose = Logs.verbose)
    except Exception as e:
        raise Exception('Error importing or using AnalyticsWebReport script: %s' % e)


def create_ndr_report(task):
    # TODO: get the data
    pass



def build(bld):
    bld(rule=my_copy, target='symbols.lang')
    bld(rule=my_copy, target='nginx_if_cfg.txt')

    for x in bld.path.ant_glob('images\\**\**.png'):
            bld(rule=my_copy, target=x)
            bld.add_group() 


    for x in bld.path.ant_glob('yaml\\**\**.yaml'):
            bld(rule=my_copy, target=x)
            bld.add_group() 



    for x in bld.path.ant_glob('video\\**\**.mp4'):
            bld(rule=my_copy, target=x)
            bld.add_group() 


    for x in bld.path.ant_glob('images\\**\**.jpg'):
        bld(rule=my_copy, target=x)
        bld.add_group() 

    if bld.options.performance or bld.options.performance_detailed:
        bld(rule=create_analytic_report)
        bld.add_group()
        bld(rule=convert_to_html_toc_book, source='trex_analytics.asciidoc waf.css', target='trex_analytics.html',scan=ascii_doc_scan);
        return

    if bld.options.ndr:
        bld(rule=create_ndr_report)
        bld.add_group()
        bld(rule=convert_to_html_toc_book, source='trex_ndr_benchmark.asciidoc waf.css', target='trex_ndr_benchmark.html',scan=ascii_doc_scan);
        return

    bld(rule=my_copy, target='my_chart.js')

    build_cp(bld,'hlt_args.asciidoc','stl/trex_stl_lib', parse_hlt_args)

    bld.add_group() # separator, the documents may require any of the pictures from above

    if os.path.exists('build/hlt_args.asciidoc'):
        bld.add_manual_dependency(
            bld.path.find_node('trex_stateless.asciidoc'),
            b'build/hlt_args.asciidoc')

    bld(rule='${ASCIIDOC}  -f ../backends/deckjs/deckjs.conf -o ${TGT} ${SRC[0].abspath()}',
        source='trex_config.asciidoc ', target='trex_config_guide.html', scan=ascii_doc_scan)


    bld(rule='${ASCIIDOC}  -f ../backends/deckjs/deckjs.conf -o ${TGT} ${SRC[0].abspath()}',
        source='trex_preso.asciidoc ', target='trex_preso.html', scan=ascii_doc_scan)

    bld(rule='${ASCIIDOC}  -a stylesheet=${SRC[1].abspath()} -a  icons=true -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='release_notes.asciidoc waf.css', target='release_notes.html', scan=ascii_doc_scan)
                
    bld(rule='${ASCIIDOC} -a docinfo -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2  -a max-width=55em  -d book   -o ${TGT} ${SRC[0].abspath()}',
        source='draft_trex_stateless.asciidoc waf.css', target='draft_trex_stateless.html', scan=ascii_doc_scan)


    bld(rule='${ASCIIDOC} -a docinfo -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2  -a max-width=55em  -d book   -o ${TGT} ${SRC[0].abspath()}',
        source='draft_trex_stateless_moved1.asciidoc waf.css', target='draft_trex_stateless1.html', scan=ascii_doc_scan)

    bld(rule=convert_to_pdf_book,source='trex_book.asciidoc waf.css', target='trex_book.pdf', scan=ascii_doc_scan)

    bld(rule=convert_to_pdf_book,source='trex_stateless.asciidoc waf.css', target='trex_stateless.pdf', scan=ascii_doc_scan)

    bld(rule=convert_to_pdf_book,source='draft_trex_stateless.asciidoc waf.css', target='draft_trex_stateless.pdf', scan=ascii_doc_scan)

    bld(rule=convert_to_pdf_book,source='trex_vm_manual.asciidoc waf.css', target='trex_vm_manual.pdf', scan=ascii_doc_scan)

    bld(rule=convert_to_pdf_book,source='trex_control_plane_peek.asciidoc waf.css', target='trex_control_plane_peek.pdf', scan=ascii_doc_scan)

    bld(rule=convert_to_pdf_book, source='trex_control_plane_design_phase1.asciidoc waf.css', target='trex_control_plane_design_phase1.pdf', scan=ascii_doc_scan)

    # with nice TOC 
    bld(rule=convert_to_html_toc_book,
        source='trex_vm_manual.asciidoc waf.css', target='trex_vm_manual.html',scan=ascii_doc_scan)

    bld(rule=convert_to_html_toc_book,
        source='trex_vm_bench.asciidoc waf.css', target='trex_vm_bench.html',scan=ascii_doc_scan)

    bld(rule=convert_to_asciidoctor_chunk_book,
                    source='trex_cookbook.asciidoc waf.css', target='trex_cookbook',scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book,
        source='trex_stateless.asciidoc waf.css', target='trex_stateless.html',scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book,
        source='trex_astf.asciidoc waf.css', target='trex_astf.html',scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book_disqus,
        source='trex_astf_vs_nginx.asciidoc waf.css', target='trex_astf_vs_nginx.html',scan=ascii_doc_scan);

    bld(rule=convert_to_pdf_book,source='trex_astf.asciidoc waf.css', target='trex_astf.pdf', scan=ascii_doc_scan)

    bld(rule=convert_to_html_toc_book,
        source='trex_stateless_bench.asciidoc waf.css', target='trex_stateless_bench.html',scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book,
        source='trex_stateless_wlc_bench.asciidoc waf.css', target='trex_stateless_wlc_bench.html', scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book,
        source='trex_book.asciidoc waf.css', target='trex_manual.html',scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book,
        source='trex_faq.asciidoc waf.css', target='trex_faq.html',scan=ascii_doc_scan);

    bld(rule=convert_to_html_toc_book,
        source='trex_rpc_server_spec.asciidoc waf.css', target='trex_rpc_server_spec.html',scan=ascii_doc_scan);
        
    bld(rule=convert_to_html_toc_book,
        source='trex_scapy_rpc_server.asciidoc waf.css', target='trex_scapy_rpc_server.html',scan=ascii_doc_scan);
    
    bld(rule=convert_to_html_toc_book,
        source='analyticsBlog.asciidoc waf.css', target='analyticsBlog.html',scan=ascii_doc_scan);

    bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='vm_doc.asciidoc waf.css', target='vm_doc.html', scan=ascii_doc_scan)

    bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='packet_builder_yaml.asciidoc waf.css', target='packet_builder_yaml.html', scan=ascii_doc_scan)


    bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='trex_control_plane_design_phase1.asciidoc waf.css', target='trex_control_plane_design_phase1.html', scan=ascii_doc_scan)
        
    bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='trex_control_plane_peek.asciidoc waf.css', target='trex_control_plane_peek.html', scan=ascii_doc_scan)

    bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='trex_console.asciidoc waf.css', target='trex_console.html', scan=ascii_doc_scan)

    bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true  -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
        source='trex_index.asciidoc waf.css', target='index.html', scan=ascii_doc_scan)

    build_cp(bld,'cp_docs','doc',build_cp_docs)

    build_cp(bld,'cp_stl_docs','doc_stl',build_stl_cp_docs)

    build_cp(bld,'cp_astf_docs','doc_astf',build_astf_cp_docs)


class Env(object):
    @staticmethod
    def get_env(name) :
        s= os.environ.get(name);
        if s == None:
            print("You should define $",name)
            raise Exception("Env error");
        return (s);
    
    @staticmethod
    def get_release_path () :
        s= Env().get_env('TREX_LOCAL_PUBLISH_PATH');
        s +=get_build_num ()+"/"
        return  s;

    @staticmethod
    def get_remote_release_path () :
        s= Env().get_env('TREX_REMOTE_PUBLISH_PATH');
        return  s;

    @staticmethod
    def get_local_web_server () :
        s= Env().get_env('TREX_WEB_SERVER');
        return  s;

    # extral web 
    @staticmethod
    def get_trex_ex_web_key() :
        s= Env().get_env('TREX_EX_WEB_KEY');
        return  s;

    @staticmethod
    def get_trex_ex_web_path() :
        s= Env().get_env('TREX_EX_WEB_PATH');
        return  s;

    @staticmethod
    def get_trex_ex_web_user() :
        s= Env().get_env('TREX_EX_WEB_USER');
        return  s;

    @staticmethod
    def get_trex_ex_web_srv() :
        s= Env().get_env('TREX_EX_WEB_SRV');
        return  s;

    @staticmethod
    def get_trex_core() :
        s= Env().get_env('TREX_CORE_GIT');
        return  s;



def release(bld):
    # copy all the files to our web server 
    core_dir = Env().get_trex_core()
    release_dir = core_dir +"/scripts/doc/";
    os.system('mkdir -p '+release_dir)
    os.system('cp -rv build/release_notes.* '+ release_dir)


def rsync_int(bld, src, dst):
    cmd = 'rsync -av --del --rsh=ssh build/{src} {host}:{dir}/{dst}'.format(
           src = src,
           host = Env().get_local_web_server(),
           dir = Env().get_remote_release_path() + '../doc',
           dst = dst)
    ret = os.system(cmd)
    if ret:
        bld.fatal("cmd '%s' exited with return status: %s" % (cmd, ret))


def rsync_ext(bld, src, dst):
    cmd = 'rsync -avz --del -e "ssh -i {key}" --rsync-path=/usr/bin/rsync build/{src} {user}@{host}:{dir}/doc/{dst}'.format(
           key  = Env().get_trex_ex_web_key(),
           src  = src,
           user = Env().get_trex_ex_web_user(),
           host = Env().get_trex_ex_web_srv(),
           dir  = Env().get_trex_ex_web_path(),
           dst  = dst)
    ret = os.system(cmd)
    if ret:
        bld.fatal("cmd '%s' exited with return status: %s" % (cmd, ret))


def publish(bld):
    # copy all the files to internal web server 
    rsync_int(bld, '', '')


def publish_ext(bld):
    # copy all the files to external web server 
    rsync_ext(bld, '', '')


def publish_perf(bld):
    # copy performance files to internal and external servers
    rsync_int(bld, 'trex_analytics.html', '')
    rsync_ext(bld, 'trex_analytics.html', '')
    rsync_int(bld, 'trex_analytics.json', '')
    rsync_ext(bld, 'trex_analytics.json', '')
    rsync_int(bld, 'images/*_latest_test_*', 'images/')
    rsync_ext(bld, 'images/*_latest_test_*', 'images/')
    rsync_int(bld, 'images/*_trend_graph.*', 'images/')
    rsync_ext(bld, 'images/*_trend_graph.*', 'images/')
    rsync_int(bld, 'images/*_trend_stats.*', 'images/')
    rsync_ext(bld, 'images/*_trend_stats.*', 'images/')
    rsync_int(bld, 'images/_detailed_table.csv', 'images/')
    rsync_ext(bld, 'images/_detailed_table.csv', 'images/')

    rsync_int(bld, 'images/_comparison.png', 'images/')
    rsync_ext(bld, 'images/_comparison.png', 'images/')

    rsync_int(bld, 'images/_comparison_stats_table.csv', 'images/')
    rsync_ext(bld, 'images/_comparison_stats_table.csv', 'images/')
    


def publish_test(bld):
    # copy all the files to our web server 
    remote_dir = "%s:%s" % ( Env().get_local_web_server(), Env().get_remote_release_path ()+'../test/')
    os.system('rsync -av --del --rsh=ssh build/ %s' % (remote_dir))



def publish_both(bld):
    publish(bld)
    publish_ext(bld)

         
def test(bld):
    # copy all the files to our web server 
    #toc_fixup_file ('build/trex_stateless.tmp',
    #                'build/trex_stateless.html',
    #                'trex_stateless.json')

    print build_disqus("my_html")
  



