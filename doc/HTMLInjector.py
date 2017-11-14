import lxml.html as LH
from StringIO import StringIO
from copy import deepcopy

class HTMLInjector(object):
    TOC_BEGIN = """
    
    <body class="book">
    <div id="toc-section">
            <div id="toctitle">
                <img class="trex_logo" src="$IMGPATH$/images/trex_logo_toc.png"/>
                Table of Contents
            </div>
    
            <div id="toggle">
              <img src="$IMGPATH$/images/icons/toggle.png" title="click to toggle table of contents"/>
            </div>
            
            <div id="toc">
              <div id="nav-tree">
              
              </div>
            </div>
    </div>
    
    <div id="content-section">
    
        <!-- load the theme CSS file -->
        <link href="https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css" rel="stylesheet"/>
        
        <link href="https://code.jquery.com/ui/1.9.2/themes/base/jquery-ui.css" rel="stylesheet" />
        
        <!-- include the jQuery library -->
        <script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js">
        </script>
        
        <!-- include the jQuery UI library -->
        <script src="https://cdnjs.cloudflare.com/ajax/libs/jqueryui/1.11.4/jquery-ui.min.js">
        </script>
        
        <!-- include the minified jstree source -->
        <script src="https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/jstree.min.js">
        </script>
    
        <!-- Hide TOC on mobile -->
        <script>
        
          // Hide TOC when it is mobile
          checkMobile();
        
          // Hide TOC by default if it is mobile
          function checkMobile(){
            if(isMobileDevice()){
              hideTOC();
            }
          }
        
          // Check it it it is running on mobile device
          function isMobileDevice() {
              if(
                  navigator.userAgent.match(/Android/i) ||
                  navigator.userAgent.match(/BlackBerry/i) ||
                  navigator.userAgent.match(/iPhone|iPad|iPod/i) ||
                  navigator.userAgent.match(/Opera Mini/i) ||
                  navigator.userAgent.match(/IEMobile/i) ||
                  navigator.userAgent.match(/iPhone|iPad|iPod/i)
                )
              {
                return true;
              }
              else
              {
                return false;
              }
          }
        
          // Hide TOC - for the first time in mobile
          function hideTOC(){
              $("#toc").hide();
              $("#toctitle").hide();
              // Show the show/hide button
              $("#toggle").css("right", "-40px");
              // Fil width
              $("body").css("margin-left", "50px");
          }
        
        </script>
    
      <div id="content-section-inner">
    
    """
    
    TOC_END = """

      <!--BODYEND-->
      
      </div>
      <!-- End Of Inner Content Section -->
    </div>
    <!-- End of Content Section -->
    
    </body>
    
    <style type="text/css">
        #toc {
          margin-bottom: 2.5em;
        }
        
        #toctitle {
          color: #527bbd;
          font-size: 1.1em;
          font-weight: bold;
          margin-top: 1.0em;
          margin-bottom: 0.1em;
        }
    
        @media screen {
          body {
            margin-left: 20em;
          }
        
          #toc {
            position: fixed;
            top: 51px;
            left: 0;
            bottom: 0;
            width: 18em;
            padding-bottom: 1.5em;
            margin: 0;
            overflow-x: auto !important;
            overflow-y: auto !important;
            border-right: solid 2px #cfcfcf;
            background-color: #FAFAFA;
            white-space: nowrap;
          }
        
          #toctitle {
            font-size: 17px !important;
            color: #4d4d4d !important;
            margin-top: 0px;
            height: 36px;
            line-height: 36px;
            background-color: #e4e2e2;
            padding: 8px 0px 7px 45px;
            white-space: nowrap;
            left: 0px;
            display: block;
            position: fixed;
            z-index: 100;
            width: 245px;
            top: 0px;
            overflow: hidden;
          }
          
          #toc .toclevel1 {
            margin-top: 0.5em;
          }
        
          #toc .toclevel2 {
            margin-top: 0.25em;
            display: list-item;
            color: #aaaaaa;
          }
        
        }
    
    
      /* Custom for Nave Tree */
      #nav-tree{
        margin-left: 10px !important;
      }
      
      #nav-tree ul > li {
        color: #000 !important;
      }
      
      .jstree-wholerow.jstree-wholerow-clicked {
        background-image: url('$IMGPATH$/images/icons/selected_tab_bg.png');
        background-repeat: repeat-x;
        color: #fff !important;
        text-shadow: 0px 1px 1px rgba(0, 0, 0, 1.0);
      }
    
            /* For side bar */
      .ui-resizable-e{
        height: 100%;
        width: 4px !important;
        position: fixed !important;
        top: 0px !important;
        cursor: e-resize !important;
        background: url('$IMGPATH$/images/splitbar.png') repeat scroll right center transparent !important;
      }
    
      .jstree-default .jstree-themeicon{
        display: none !important;
      }
    
    
      .jstree-anchor {
        font-size: 12px !important;
        color: #91A501 !important;
      }
    
    
      .jstree-clicked{
        color: white !important;
      }
      
    
      #toggle {
        position: fixed;
        top: 14px;
        left: 10px;
        z-index: 210;
        width: 24px;
      }
    
      #toggle img {
        opacity:0.3;
      }
    
      #toggle img:hover {
        opacity:0.9;
      }
    
      .trex_logo{
        top: 6px;
        position: relative;
      }
    
       html{
        overflow: hidden;
      }
    
      body{
        margin-right: 0px !important;
        margin-top: 0px !important;
        margin-bottom: 0px !important;
      }
    
      #toc-section{
        position: absolute;
        z-index: 200;
      }
    
      #content-section{
        overflow: auto;
      }
    
      #content-section-inner{
        max-width: 50em;
      }
    
    
     </style>
    
    
    
    <script>
    
          $(document).ready(function(){
              var isOpen = true;
    
            // Initialize NavTree
            initializeNavTree();
            // Drag TOC left and right
            initResizable();
            // Toggle TOC whe clicking on the menu icon
            toggleTOC();
            // Handle Mobile - close TOC
            checkMobile();
    
            function initializeNavTree() {
    
              // TOC tree options
              var toc_tree = $('#nav-tree');        
    
              var toc_tree_options = {
                'core' : {
                  "animation" :false,
                  "themes" : { "stripes" : false },
                  'data' : {
                    "url" : "./input_replace_me.json",  
                    "dataType" : "json" // needed only if you do not supply JSON headers
                  }
                }
                ,
                "plugins" : [ "wholerow" ]
              };
    
              $('#nav-tree').jstree(toc_tree_options) ;
    
              toc_tree.on("changed.jstree", function (e, data) {
              
                // filename
                var filename = data.instance.get_selected(true)[0].original.link
              
                $ONCLICK$
              });
              }
            
              
            // a hook for selecting the first node if needed
            $SELECTFIRST$
              
            function initResizable() {
              var toc = $("#toc");
              var body = $("body");
    
              // On resize
              $("#toc").resizable({
                  resize: function(e, ui) {
                      resized();
                  },
                  handles: 'e'
              });
              
            // On zoom changed
              $(window).resize(function() {
                if(isOpen){
                    resized();
                }
              });
    
    
             // Do it for the first time
                var tocWidth = $(toc).outerWidth();
                var windowHeight = $(window).height();
                $(".ui-resizable-e").css({"right":$(window).width()-parseInt(tocWidth)+"px"});
                $("#toctitle").css({"width":parseInt(tocWidth)-45+"px"});
                $("#toc-section").css({"height":windowHeight + "px"});
                $("#content-section").css({"height":windowHeight + "px"});
           
            }
    
            function resized(){
              var body = $("body");
              var tocWidth = $(toc).outerWidth();
              var windowHeight = $(window).height();
    
              body.css({"marginLeft":parseInt(tocWidth)+20+"px"});
              $(".ui-resizable-e").css({"right":$(window).width()-parseInt(tocWidth)+"px"});
              $("#toctitle").css({"width":parseInt(tocWidth)-45+"px"});
              $("#toc-section").css({"height":windowHeight + "px"});
              $("#content-section").css({"height":windowHeight + "px"});
              
            }
    
    
            function toggleTOC(){
              $( "#toggle" ).click(function() {
                if ( isOpen ) {
                  // Close it
                   closTOC();
                } else {
                  // Open it
                  openTOC();
                }
                // Toggle status
                isOpen = !isOpen;
              });
            }
    
    
            // Close TOC by default if it is mobile
            function checkMobile(){
              if(isMobileDevice()){
                isOpen=false;
                $(".ui-resizable-e").hide();
              }
            }
    
            // Check it it it is running on mobile device
            function isMobileDevice() {
                if(
                    navigator.userAgent.match(/Android/i) ||
                    navigator.userAgent.match(/BlackBerry/i) ||
                    navigator.userAgent.match(/iPhone|iPad|iPod/i) ||
                    navigator.userAgent.match(/Opera Mini/i) ||
                    navigator.userAgent.match(/IEMobile/i) || 
                    navigator.userAgent.match(/iPhone|iPad|iPod/i)
                  )
                {
                  return true;
                }
                else
                {
                  return false;
                }
            }
    
            // Close TOC
            function closTOC(){
                $("#toc").hide("slide", 500);
                $("#toctitle").hide("slide", 500);
                if(!isMobileDevice()){
                    $(".ui-resizable-e").hide("slide", 500);
                }
                // Show the show/hide button
                $("#toggle").css("right", "-40px");
                // Fil width
                $("body").animate({"margin-left": "50px"}, 500);
            }
    
            // Open TOC
            function openTOC(){
                $("#toc").show("slide", 500);
                $("#toctitle").show("slide", 500);
                if(!isMobileDevice()){
                  $(".ui-resizable-e").show("slide", 500);
                }
                // Show the show/hide button
                $("#toggle").css("right", "15px");
                // Minimize page width
                $("body").animate({"margin-left": $(toc).outerWidth()+20+"px"}, 500);
            }
    
          });
    
    </script>
    
    
    
    """
    
    ONCLICK_MULTIPAGE = """
    $.get(filename, function(r) {
       
    // replace content
    var els = $(r).find('.sect1');
    $('#content').html(els);
    });
    """
    
    ONCLICK_SINGLEPAGE = "window.location.href = filename"
    
    DISQUS_HTML = """

    <div id="disqus_thread"></div>
    <script>

    /**
    *  RECOMMENDED CONFIGURATION VARIABLES: EDIT AND UNCOMMENT THE SECTION BELOW TO INSERT DYNAMIC VALUES FROM YOUR PLATFORM OR CMS.
    *  LEARN WHY DEFINING THESE VARIABLES IS IMPORTANT: https://disqus.com/admin/universalcode/#configuration-variables*/
    
    if (typeof(disqus_config) == "undefined") {
        var disqus_config = function () {
        this.page.url = window.location.href + "$ID$"
        this.page.identifier = "$ID$";
        this.page.title = "$ID$";
        };
    
        (function() { // DON'T EDIT BELOW THIS LINE
        var d = document, s = d.createElement('script');
        s.src = 'https://trex-tgn.disqus.com/embed.js';
        s.setAttribute('data-timestamp', +new Date());
        (d.head || d.body).appendChild(s);
        })();
    } else {
    
        /* * * Disqus Reset Function * * */
        var disqus_reset = function (newIdentifier, newUrl, newTitle, newLanguage) {
         DISQUS.reset({
                reload: true,
                config: function () {
                this.page.identifier = newIdentifier;
                this.page.url = newUrl;
                this.page.title = newTitle;
                this.language = newLanguage;
             }
        });
        };
            
        disqus_reset("$ID$", window.location.href + "$ID$", "$ID$", 'en')
    
    }
    
    
    </script>
    <noscript>Please enable JavaScript to view the <a href="https://disqus.com/?ref_noscript">comments powered by Disqus.</a></noscript>

    """
    
    
    SELECT_FIRST = """
          // on loaded event  
          $('#nav-tree').on('loaded.jstree', function() {
              $('#nav-tree').jstree('select_node', 'j1_1');
              $("#nav-tree").jstree('open_all');
          });
    """
    
    
    def __init__ (self, input_filename, output_filename = None):
        """
            Open filename for HTML injection

            input_filename  - file to process
            output_filename - if non provided will override input
        """

        self.input_filename  = input_filename
        self.output_filename = output_filename or input_filename


    def __enter__ (self):

        # read all the data from file as string
        with open(self.input_filename) as f:
            self.xml_doc = LH.parse(StringIO(f.read()))

        return self


    def __exit__ (self, exc_type, exc_val, exc_tb):
        # save to file
        with open(self.output_filename, "w") as f:
            f.write(LH.tostring(self.xml_doc))


    def inject_toc (self,
                    toc_json_filename,
                    fmt = "single",
                    image_path = "."):
        """
            Inject TOC to an input file
            
            toc_json_file   - the JSON defining the TOC hierarchy
            fmt             - can be 'single' for single page or 'multi' for multipage
            image_path      - where can the images for the TOC be found ?
            
        """

        toc_begin = HTMLInjector.TOC_BEGIN
        toc_end   = HTMLInjector.TOC_END
        
        # fix image path
        toc_begin = toc_begin.replace('$IMGPATH$', image_path)
        toc_end   = toc_end.replace('$IMGPATH$', image_path)
        
        # fill up TOC JSON file
        toc_end = toc_end.replace('input_replace_me.json', toc_json_filename)
        
        # on click event
        toc_end = toc_end.replace('$ONCLICK$', HTMLInjector.ONCLICK_MULTIPAGE if fmt == 'multi' else HTMLInjector.ONCLICK_SINGLEPAGE)
        
        # select first on multipage
        toc_end = toc_end.replace('$SELECTFIRST$', HTMLInjector.SELECT_FIRST if fmt == 'multi' else '')
        
        # create TOC node
        toc_node  = LH.parse(StringIO(toc_begin + toc_end)).getroot()
        
        # find the main node in the HTML page
        main_node = (self.xml_doc.xpath('//body[@class = "article"]') or self.xml_doc.xpath('//body[@class = "book"]'))[0]
        
        # find the right place to embed the data
        toc_csi   = toc_node.xpath('//div[@id = "content-section-inner"]')[0]

        # take all the children of the main node and embed them in the CSI
        for elem in main_node.getchildren():
            toc_csi.append(deepcopy(elem))

        # final step - replace the body element with the TOC embedded element
        main_node.getparent().replace(main_node, toc_node)
        
            

    def inject_disqus (self, page_id):
        """
            Inject disqus to HTML
            if the page has <disqus></disqus> it will be injected with disqus code

        """
     
        disqus_injection_place = self.xml_doc.findall('.//disqus')
        if len(disqus_injection_place) == 0:
            return
        elif len(disqus_injection_place) > 1:
            raise Exception('Multiple disqus components in HTML page - aborting')


        # take the first (and only) element
        disqus_injection_place = disqus_injection_place[0]

        # create the disqus node
        disqus_node = LH.parse(StringIO(self.DISQUS_HTML.replace("$ID$", page_id))).getroot()

        # inject
        disqus_injection_place.getparent().replace(disqus_injection_place, disqus_node)



    def inject_title (self, title):

        title_elem = self.xml_doc.find(".//title")
        if title_elem is None:
            raise Exception('unable to find title')

        title_elem.text = title
            

    def clear_class (self, class_name):

        c = self.xml_doc.xpath('.//div[@class = "{0}"]'.format(class_name))
        if not c:
            raise Exception('unable to find class {0}'.format(class_name))

        c = c[0]
        for child in c.getchildren():
            c.remove(child)


