/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2010 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include <osgEarthSymbology/Style>
#include <osgEarthSymbology/CssUtils>
#include <osgEarthSymbology/SLD>
#include <osgEarth/HTTPClient>
#include <algorithm>

using namespace osgEarth;
using namespace osgEarth::Symbology;

//------------------------------------------------------------------------

#undef  LC
#define LC "[Style] "

Style::Style( const std::string& name ) :
_name( name )
{
    //NOP
}

Style::Style(const Style& rhs, const osg::CopyOp& op) :
_name    ( rhs._name ),
_symbols ( rhs._symbols ),
_origType( rhs._origType ),
_origData( rhs._origData ),
_uri     ( rhs._uri )
{
    if ( op.getCopyFlags() == osg::CopyOp::SHALLOW_COPY )
    {
        _symbols = rhs._symbols;
    }
    else
    {
        _symbols.clear();
        mergeConfig( rhs.getConfig(false) );
    }
}

Style&
Style::operator = ( const Style& rhs )
{
    _name.clear();
    _origType.clear();
    _origData.clear();
    _uri.unset();
    _symbols.clear();
    mergeConfig( rhs.getConfig(false) );
    return *this;
}

void Style::addSymbol(Symbol* symbol)
{
    if ( symbol )
        _symbols.push_back(symbol);
}

bool Style::removeSymbol(Symbol* symbol)
{
    SymbolList::iterator it = std::find(_symbols.begin(), _symbols.end(), symbol);
	if (it == _symbols.end())
		return false;
		
	_symbols.erase(it);
	
	return true;
}

Style::Style( const Config& conf )
{
    mergeConfig( conf );
}

Style
Style::combineWith( const Style& rhs ) const
{
    // start by deep-cloning this style.
    Config conf = getConfig( false );
    Style newStyle( conf );

    // next, merge in the symbology from the other style.
    newStyle.mergeConfig( rhs.getConfig(false) );

    if ( !this->empty() && !rhs.empty() )
        newStyle.setName( _name + std::string(":") + rhs.getName() );
    else if ( !this->empty() && rhs.empty() )
        newStyle.setName( _name );
    else if ( this->empty() && !rhs.empty() )
        newStyle.setName( rhs.getName() );
    else
        newStyle.setName( _name );

    return newStyle;
}

void
Style::mergeConfig( const Config& conf )
{
    if ( _name.empty() )
        _name = conf.value( "name" );

    // if there's no explicit name, use the KEY as the name.
    if ( _name.empty() )
        _name = conf.key();

    conf.getIfSet( "url", _uri ); // named "url" for back compat

    _origType = conf.value( "type" );

    if ( conf.value( "type" ) == "text/css" )
    {
        _origData = conf.value();
        
        // just take the first block.
        ConfigSet blocks;
        CssUtils::readConfig( _origData, conf.referrer(), blocks );
        if ( blocks.size() > 0 )
            SLDReader::readStyleFromCSSParams( blocks.front(), *this );
    }
    else
    {
        Config symbolConf = conf.child( "symbols" );
        if ( !symbolConf.empty() )
        {
            for( ConfigSet::const_iterator i = symbolConf.children().begin(); i != symbolConf.children().end(); ++i )
            {
                const Config& c = *i;

                if ( c.key() == "text" )
                {
                    getOrCreate<TextSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "point" )
                {
                    getOrCreate<PointSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "line" )
                {
                    getOrCreate<LineSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "polygon" )
                {
                    getOrCreate<PolygonSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "extrusion" )
                {
                    getOrCreate<ExtrusionSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "altitude" )
                {
                    getOrCreate<AltitudeSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "marker" )
                {
                    getOrCreate<MarkerSymbol>()->mergeConfig( c );
                }
                else if ( c.key() == "skin" )
                {
                    getOrCreate<SkinSymbol>()->mergeConfig( c );
                }
            }
        }
    }
}

Config
Style::getConfig( bool keepOrigType ) const
{
    Config conf( "style" );
    conf.set("name", _name);

    conf.addIfSet( "url", _uri );
    
    if ( _origType == "text/css" && keepOrigType )
    {
        conf.set("type", _origType);
        conf.value() = _origData;            
    }
    else
    {
        Config symbolsConf( "symbols" );
        for( SymbolList::const_iterator i = _symbols.begin(); i != _symbols.end(); ++i )
        {
            symbolsConf.add( i->get()->getConfig() );
        }
        conf.add( symbolsConf );
    }

    return conf;
}

//------------------------------------------------------------------------

#undef  LC
#define LC "[StyleSelector] "

StyleSelector::StyleSelector( const Config& conf )
{
    mergeConfig( conf );
}

std::string
StyleSelector::getSelectedStyleName() const 
{
    return _styleName.isSet() ? *_styleName : _name;
}

void
StyleSelector::mergeConfig( const Config& conf )
{
    _name = conf.value( "name" );
    conf.getIfSet( "style", _styleName ); // backwards compatibility
    conf.getIfSet( "class", _styleName );
    conf.getObjIfSet( "query", _query );
}

Config
StyleSelector::getConfig() const
{
    Config conf( "selector" );
    conf.add( "name", _name );
    conf.addIfSet( "class", _styleName );
    conf.addObjIfSet( "query", _query );
    return conf;
}

//------------------------------------------------------------------------

#undef  LC
#define LC "[StyleSheet] "

StyleSheet::StyleSheet( const Config& conf )
{
    mergeConfig( conf );
}

void
StyleSheet::addStyle( const Style& style )
{
    _styles[ style.getName() ] = style;
}

void
StyleSheet::removeStyle( const std::string& name )
{
    _styles.erase( name );
}

Style*
StyleSheet::getStyle( const std::string& name, bool fallBackOnDefault )
{
    StyleMap::iterator i = _styles.find( name );
    if ( i != _styles.end() ) {
        return &i->second;
    }
    else if ( name.length() > 1 && name.at(0) == '#' ) {
        std::string nameWithoutHash = name.substr( 1 );
        return getStyle( nameWithoutHash, fallBackOnDefault );
    }
    else if ( fallBackOnDefault ) {
        return getDefaultStyle();
    }
    else {
        return 0L;
    }
}

const Style*
StyleSheet::getStyle( const std::string& name, bool fallBackOnDefault ) const
{
    StyleMap::const_iterator i = _styles.find( name );
    if ( i != _styles.end() ) {
        return &i->second;
    }
    else if ( name.length() > 1 && name.at(0) == '#' ) {
        std::string nameWithoutHash = name.substr( 1 );
        return getStyle( nameWithoutHash, fallBackOnDefault );
    }
    else if ( fallBackOnDefault ) {
        return getDefaultStyle();
    }
    else {
        return 0L;
    }
}

Style*
StyleSheet::getDefaultStyle()
{
    if ( _styles.find( "default" ) != _styles.end() ) {
        return &_styles.find( "default" )->second;
    }
    else if ( _styles.find( "" ) != _styles.end() ) {
        return &_styles.find( "" )->second;
    }
    if ( _styles.size() > 0 ) {
        return &_styles.begin()->second;
    }
    else {
        // insert the empty style and return it.
        _styles["default"] = _emptyStyle;
        return &_styles.begin()->second;
    }
}

const Style*
StyleSheet::getDefaultStyle() const
{
    if ( _styles.size() == 1 ) {
        return &_styles.begin()->second;
    }
    else if ( _styles.find( "default" ) != _styles.end() ) {
        return &_styles.find( "default" )->second;
    }
    else if ( _styles.find( "" ) != _styles.end() ) {
        return &_styles.find( "" )->second;
    }
    else {
        return &_emptyStyle;
    }
}

void
StyleSheet::addResourceLibrary( ResourceLibrary* lib )
{
    Threading::ScopedWriteLock exclusive( _resLibsMutex );
    _resLibs[ lib->getName() ] = lib;
}

ResourceLibrary*
StyleSheet::getResourceLibrary( const std::string& name, const osgDB::Options* dbOptions ) const
{
    Threading::ScopedReadLock shared( const_cast<StyleSheet*>(this)->_resLibsMutex );
    ResourceLibraries::const_iterator i = _resLibs.find( name );
    if ( i != _resLibs.end() )
        return i->second.get();
    else
        return 0L;
}

Config
StyleSheet::getConfig() const
{
    Config conf;

    for( StyleSelectorList::const_iterator i = _selectors.begin(); i != _selectors.end(); ++i )
    {
        conf.add( "selector", i->getConfig() );
    }

    for( StyleMap::const_iterator i = _styles.begin(); i != _styles.end(); ++i )
    {
        conf.add( "style", i->second.getConfig() );
    }

    {
        Threading::ScopedReadLock shared( const_cast<StyleSheet*>(this)->_resLibsMutex );

        for( ResourceLibraries::const_iterator i = _resLibs.begin(); i != _resLibs.end(); ++i )
        {
            if ( i->second.valid() )
            {
                Config libConf = i->second->getConfig();
                conf.add( "library", libConf );
            }
        }
    }

    if ( _script.valid() )
    {
        Config scriptConf("script");
        if ( !_script->name.empty() )
            scriptConf.set( "name", _script->name );
        if ( !_script->language.empty() )
            scriptConf.set( "language", _script->language );
        if ( !_script->code.empty() )
            scriptConf.value() = _script->code;

        conf.add( scriptConf );
    }

    return conf;
}

void
StyleSheet::mergeConfig( const Config& conf )
{
    _uriContext = URIContext( conf.referrer() );

    // read in any resource library references
    ConfigSet libraries = conf.children( "library" );
    for( ConfigSet::iterator i = libraries.begin(); i != libraries.end(); ++i )
    {
        ResourceLibrary* resLib = new ResourceLibrary( *i );
        _resLibs[resLib->getName()] = resLib;
    }

    // read in any scripts
    ConfigSet scripts = conf.children( "script" );
    for( ConfigSet::iterator i = scripts.begin(); i != scripts.end(); ++i )
    {
        // get the script code
        std::string code = i->value();

        // name is optional and unused at the moment
        std::string name = i->value("name");

        std::string lang = i->value("language");
        if ( lang.empty() ) {
            // default to javascript
            lang = "javascript";
        }

        _script = new Script(code, lang, name);
    }

    // read any style class definitions. either "class" or "selector" is allowed
    ConfigSet selectors = conf.children( "selector" );
    if ( selectors.empty() ) selectors = conf.children( "class" );
    for( ConfigSet::iterator i = selectors.begin(); i != selectors.end(); ++i )
    {
        _selectors.push_back( StyleSelector( *i ) );
    }

    // read in the actual styles
    ConfigSet styles = conf.children( "style" );
    for( ConfigSet::iterator i = styles.begin(); i != styles.end(); ++i )
    {
        const Config& styleConf = *i;

        if ( styleConf.value("type") == "text/css" )
        {
            // for CSS data, there may be multiple styles in one CSS block. So
            // parse them all out and add them to the stylesheet.

            // read the inline data:
            std::string cssString = styleConf.value();

            // if there's a URL, read the CSS from the URL:
            if ( styleConf.hasValue("url") )
            {
                URI uri( styleConf.value("url"), styleConf.referrer() );
                cssString = uri.readString().getString();
            }

            // break up the CSS into multiple CSS blocks and parse each one individually.
            std::vector<std::string> blocks;
            CssUtils::split( cssString, blocks );

            for( std::vector<std::string>::iterator i = blocks.begin(); i != blocks.end(); ++i )
            {
                Config blockConf( styleConf );
                blockConf.value() = *i;
                //OE_INFO << LC << "Style block = " << blockConf.toJSON() << std::endl;
                Style style( blockConf );
                _styles[ style.getName() ] = style;
            }
        }
        else
        {
            Style style( styleConf );
            _styles[ style.getName() ] = style;
        }
    }
}

