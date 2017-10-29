require 'asciidoctor'
require 'optparse'

class MultipageHtml5Converter
  include Asciidoctor::Converter
  include Asciidoctor::Writer

  register_for 'multipage_html5'
  EOL = "\n"

  def initialize backend, opts
    super
    basebackend 'html'
    @documents = []
  end

  def convert node, transform = nil
    transform ||= node.node_name
    send transform, node if respond_to? transform
  end

  def document node
    node.blocks.each {|b| b.convert }
    node.blocks.clear
    master_content = []
    master_content << %(= #{node.doctitle})
    master_content << ((node.attr 'author') || "")
    master_content << ''
    @documents.each do |doc|
      sect = doc.blocks[0]
      sectnum = sect.numbered && !sect.caption ? %(#{sect.sectnum} ) : nil
      master_content << %(* <<#{node.attributes['docname']}#{doc.attr 'docname'}#,#{sectnum}#{sect.captioned_title}>>)
    end
    Asciidoctor.convert master_content, :doctype => node.doctype, :header_footer => true, :safe => node.safe
  end

  def section node
    doc = node.document
    attributes = {}
    doc.attributes.each do |k, v|
      attributes[k] = v
    end
    attributes['noheader'] = ''
    attributes['doctitle'] = node.title
    attributes['backend'] = 'html5'
    page = Asciidoctor::Document.new [], :header_footer => true, :doctype => doc.doctype, :safe => doc.safe, :parse => true, :attributes => attributes
    page.set_attr 'docname', node.id
    # TODO recurse
    #node.parent = page
    #node.blocks.each {|b| b.parent = node }
    reparent node, page

    # NOTE don't use << on page since it changes section number
    page.blocks << node
    @documents << page
    ''
  end

  def reparent node, parent
    node.parent = parent
    node.blocks.each do |block|
      reparent block, node unless block.context == :dlist
      if block.context == :table
        block.columns.each do |col|
          col.parent = col.parent
        end
        block.rows.body.each do |row|
          row.each do |cell|
            cell.parent = cell.parent
          end
        end
      end
    end
  end

  #def paragraph node
  #  puts 'here'
  #end

  def write output, target
    outdir = ::File.dirname target
    bn = ::File.basename target, '.*'
    @documents.each do |doc|
      outfile = ::File.join outdir, %(#{bn}#{doc.attr 'docname'}.html)
      ::File.open(outfile, 'w') do |f|
        f.write doc.convert
      end
    end
    chunked_target = target.gsub(/(\.[^.]+)$/, '-chunked\1')
    ::File.open(chunked_target, 'w') do |f|
      f.write output
    end
  end
end

if __FILE__ == $PROGRAM_NAME
  args = {:dir => '.'}

  OptionParser.new do |opts|
    opts.banner = "Usage: #{$PROGRAM_NAME} [-D <destination_directory>] <file>"
    opts.on('-D', '--destination-dir DIR', 'destination output directory (default: directory of source file)') do |dir|
      args[:dir] = dir
    end
  end.parse!

  options = {}
  options[:header_footer] = true
  options[:backend] = :multipage_html5
  options[:safe] = :safe
  #options[:to_file] = false
  options[:to_dir] = args[:dir]
  options[:attributes] = {'icons' => 'font'}
  Asciidoctor.convert_file ARGV.first, options
end
