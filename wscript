#! /usr/bin/env python
# encoding: utf-8
# hhaim, 2014 (IL) base on WAF book

"""
call 'waf --targets=waf.pdf' or use 'waf list' to see the targets available
"""

VERSION='0.0.1'
APPNAME='wafdocs'

import os, re, shutil
import shlex
import subprocess


top = '.'
out = 'build'

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
        
        

import re
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

def configure(conf):
	conf.find_program('asciidoc', path='/usr/local/bin/', var='ASCIIDOC')
	pass;

def convert_to_pdf(task):
    input_file = task.outputs[0].abspath()
    out_dir = task.outputs[0].parent.get_bld().abspath()
    os.system('a2x --no-xmllint -v -f pdf  -d  article %s -D %s ' %(task.inputs[0].abspath(),out_dir ) )
    return (0)

def convert_to_pdf_book(task):
    input_file = task.outputs[0].abspath()
    out_dir = task.outputs[0].parent.get_bld().abspath()
    os.system('a2x --no-xmllint -v -f pdf  -d book %s -D %s ' %(task.inputs[0].abspath(),out_dir ) )
    return (0)


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

def build_cp_docs (trex_src_dir, dest_dir = "_build", builder = "html"):
    build_doc_cmd = shlex.split("/usr/local/bin/sphinx-build -b {bld} {src} {dst}".format(
        bld= builder, 
        src= ".", 
        dst= dest_dir)
        )
    bld_path = os.path.abspath( os.path.join(trex_src_dir, 'scripts', 'automation', 'trex_control_plane', 'doc') )
    ret_val = subprocess.call(build_doc_cmd, cwd = bld_path)
    if ret_val:
        raise RuntimeError("Build operation of control plain docs failed with return value {ret}".format(ret= ret_val))
    return


def build(bld):
	bld(rule=my_copy, target='symbols.lang')

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

	bld(rule=my_copy, target='my_chart.js')

	bld.add_group() # separator, the documents may require any of the pictures from above

	bld(rule='${ASCIIDOC}  -b deckjs -o ${TGT} ${SRC[0].abspath()}',
		source='trex_config.asciidoc ', target='trex_config_guide.html', scan=ascii_doc_scan)


	bld(rule='${ASCIIDOC}  -b deckjs -o ${TGT} ${SRC[0].abspath()}',
		source='trex_preso.asciidoc ', target='trex_preso.html', scan=ascii_doc_scan)

	bld(rule='${ASCIIDOC}  -a stylesheet=${SRC[1].abspath()} -a  icons=true -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='release_notes.asciidoc waf.css', target='release_notes.html', scan=ascii_doc_scan)
                

	bld(rule='${ASCIIDOC} -a docinfo -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -d book   -o ${TGT} ${SRC[0].abspath()}',
		source='trex_book.asciidoc waf.css', target='trex_manual.html', scan=ascii_doc_scan)

        bld(rule=convert_to_pdf_book,
		source='trex_book.asciidoc waf.css', target='trex_book.pdf', scan=ascii_doc_scan)
                

	bld(rule=convert_to_pdf_book,
		source='trex_vm_manual.asciidoc waf.css', target='trex_vm_manual.pdf', scan=ascii_doc_scan)

	bld(rule=convert_to_pdf_book,
		source='trex_control_plane_peek.asciidoc waf.css', target='trex_control_plane_peek.pdf', scan=ascii_doc_scan)
	
	bld(rule=convert_to_pdf_book,
		source='trex_control_plane_design_phase1.asciidoc waf.css', target='trex_control_plane_design_phase1.pdf', scan=ascii_doc_scan)

	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='trex_vm_manual.asciidoc waf.css', target='trex_vm_manual.html', scan=ascii_doc_scan)

	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='vm_doc.asciidoc waf.css', target='vm_doc.html', scan=ascii_doc_scan)

	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='packet_builder_yaml.asciidoc waf.css', target='packet_builder_yaml.html', scan=ascii_doc_scan)
		
	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='trex_rpc_server_spec.asciidoc waf.css', target='trex_rpc_server_spec.html', scan=ascii_doc_scan)

	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='trex_control_plane_design_phase1.asciidoc waf.css', target='trex_control_plane_design_phase1.html', scan=ascii_doc_scan)
		
	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='trex_control_plane_peek.asciidoc waf.css', target='trex_control_plane_peek.html', scan=ascii_doc_scan)

	bld(rule='${ASCIIDOC}   -a stylesheet=${SRC[1].abspath()} -a  icons=true -a toc2 -a max-width=55em  -o ${TGT} ${SRC[0].abspath()}',
		source='trex_console.asciidoc waf.css', target='trex_console.html', scan=ascii_doc_scan)

	# generate control plane documentation
	export_path = os.path.join(os.getcwd(), 'build', 'cp_docs')
	trex_core_git_path = os.getenv('TREX_CORE_GIT', None)
	if trex_core_git_path: # there exists the desired ENV variable.
		build_cp_docs(trex_core_git_path, dest_dir= export_path)
	else:
		raise NameError("Environment variable 'TREX_CORE_GIT' is not defined.")


class Env(object):
    @staticmethod
    def get_env(name) :
        s= os.environ.get(name);
        if s == None:
            print "You should define $",name
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


def publish(bld):
    # copy all the files to our web server 
    remote_dir = "%s:%s" % ( Env().get_local_web_server(), Env().get_remote_release_path ()+'../doc/')
    os.system('rsync -av --rsh=ssh build/ %s' % (remote_dir))


def publish_ext(bld):
   from_ = 'build/'
   os.system('rsync -avz -e "ssh -i %s" --rsync-path=/usr/bin/rsync %s %s@%s:%s/doc/' % (Env().get_trex_ex_web_key(),from_, Env().get_trex_ex_web_user(),Env().get_trex_ex_web_srv(),Env().get_trex_ex_web_path() ) )
   




         
               

