//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2019 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#ifndef SETTINGS_HXX
#define SETTINGS_HXX

#include <map>

#include "Variant.hxx"
#include "bspf.hxx"

/**
  This class provides an interface for accessing all configurable options,
  both from the settings file and from the commandline.

  Note that options can be configured as 'permanent' or 'temporary'.
  Permanent options are ones that the app registers with the system, and
  always saves when the app exits.  Temporary options are those that are
  used when appropriate, but never saved to the settings file.

  Each c'tor (both in the base class and in any derived classes) are
  responsible for registering all options as either permanent or temporary.
  If an option isn't registered as permanent, it will be considered
  temporary and will not be saved.

  @author  Stephen Anthony
*/
class Settings
{
  public:
    /**
      Create a new settings abstract class
    */
    explicit Settings();
    virtual ~Settings() = default;

    using Options = std::map<string, Variant>;

  public:
    /**
      This method should be called to display usage information.
    */
    void usage() const;

    /**
      This method is called to load settings from the settings file,
      and apply commandline options specified by the given parameter.

      @param cfgfile  The full path to the configuration file
      @param options  A list of options that overrides ones in the
                      settings file
    */
    void load(const string& cfgfile, const Options& options);

    /**
      This method is called to save the current settings to the
      settings file.
    */
    void save(const string& cfgfile) const;

    /**
      Get the value assigned to the specified key.

      @param key  The key of the setting to lookup
      @return  The value of the setting; EmptyVariant if none exists
    */
    const Variant& value(const string& key) const;

    /**
      Set the value associated with the specified key.

      @param key   The key of the setting
      @param value The value to assign to the key
    */
    void setValue(const string& key, const Variant& value);

    /**
      Convenience methods to return specific types.

      @param key  The key of the setting to lookup
      @return  The specific type value of the variant
    */
    int getInt(const string& key) const     { return value(key).toInt();   }
    float getFloat(const string& key) const { return value(key).toFloat(); }
    bool getBool(const string& key) const   { return value(key).toBool();  }
    const string& getString(const string& key) const { return value(key).toString(); }
    const GUI::Size getSize(const string& key) const { return value(key).toSize();   }

  protected:
    /**
      Add key/value pair to specified map.  Note that these should only be called
      directly within the c'tor, to register the 'key' and set it to the
      appropriate 'value'.  Elsewhere, any derived classes should call 'setValue',
      and let it decide where the key/value pair will be saved.
    */
    void setPermanent(const string& key, const Variant& value);
    void setTemporary(const string& key, const Variant& value);

    /**
      This method will be called to load the settings from the
      platform-specific settings file.  Since different ports can have
      different behaviour here, we mark it as virtual so derived
      classes can override as needed.

      @param cfgfile  The full path to the configuration file
      @return  False on any error, else true
    */
    virtual bool loadConfigFile(const string& cfgfile);

    /**
      This method will be called to save the current settings to the
      platform-specific settings file.  Since different ports can have
      different behaviour here, we mark it as virtual so derived
      classes can override as needed.

      @param cfgfile  The full path to the configuration file
      @return  False on any error, else true
    */
    virtual bool saveConfigFile(const string& cfgfile) const;

    // Trim leading and following whitespace from a string
    static string trim(const string& str)
    {
      string::size_type first = str.find_first_not_of(' ');
      return (first == string::npos) ? EmptyString :
              str.substr(first, str.find_last_not_of(' ')-first+1);
    }

    // FIXME - Rework so that these aren't needed; hence no commenting added
    const Options& getInternalSettings() const
      { return myPermanentSettings; }
    const Options& getExternalSettings() const
      { return myTemporarySettings; }

  private:
    /**
      This method must be called *after* settings have been fully loaded
      to validate (and change, if necessary) any improper settings.
    */
    void validate();

  private:
    // Holds key/value pairs that are necessary for Stella to
    // function and must be saved on each program exit.
    Options myPermanentSettings;

    // Holds auxiliary key/value pairs that shouldn't be saved on
    // program exit.
    Options myTemporarySettings;

  private:
    // Following constructors and assignment operators not supported
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;
};

#endif
