/********************************************************************
 * (C) 2020 Michael Toutonghi
 * 
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 * 
 * Support for the Verus Data Exchange Format (VDXF)
 * 
 */

#include "vdxf.h"

std::string DATA_KEY_SEPARATOR = "::";

std::string TrimLeading(const std::string &Name, unsigned char ch)
{
    std::string nameCopy = Name;
    int removeSpaces;
    for (removeSpaces = 0; removeSpaces < nameCopy.size(); removeSpaces++)
    {
        if (nameCopy[removeSpaces] != ch)
        {
            break;
        }
    }
    if (removeSpaces)
    {
        nameCopy.erase(nameCopy.begin(), nameCopy.begin() + removeSpaces);
    }
    return nameCopy;
}

std::string TrimTrailing(const std::string &Name, unsigned char ch)
{
    std::string nameCopy = Name;
    int removeSpaces;
    for (removeSpaces = nameCopy.size() - 1; removeSpaces >= 0; removeSpaces--)
    {
        if (nameCopy[removeSpaces] != ch)
        {
            break;
        }
    }
    nameCopy.resize(nameCopy.size() - ((nameCopy.size() - 1) - removeSpaces));
    return nameCopy;
}

std::string TrimSpaces(const std::string &Name)
{
    return TrimTrailing(TrimLeading(Name, ' '), ' ');
}

// this will add the current Verus chain name to subnames if it is not present
// on both id and chain names
std::vector<std::string> ParseSubNames(const std::string &Name, std::string &ChainOut, bool displayfilter, bool addVerus)
{
    std::string nameCopy = Name;
    std::string invalidChars = "\\/:*?\"<>|";
    if (displayfilter)
    {
        invalidChars += "\n\t\r\b\t\v\f\x1B";
    }
    for (int i = 0; i < nameCopy.size(); i++)
    {
        if (invalidChars.find(nameCopy[i]) != std::string::npos)
        {
            return std::vector<std::string>();
        }
    }

    std::vector<std::string> retNames;
    boost::split(retNames, nameCopy, boost::is_any_of("@"));
    if (!retNames.size() || retNames.size() > 2)
    {
        return std::vector<std::string>();
    }

    bool explicitChain = false;
    if (retNames.size() == 2)
    {
        ChainOut = retNames[1];
        explicitChain = true;
    }    

    nameCopy = retNames[0];
    boost::split(retNames, nameCopy, boost::is_any_of("."));

    int numRetNames = retNames.size();

    std::string verusChainName = boost::to_lower_copy(VERUS_CHAINNAME);

    if (addVerus)
    {
        if (explicitChain)
        {
            std::vector<std::string> chainOutNames;
            boost::split(chainOutNames, ChainOut, boost::is_any_of("."));
            std::string lastChainOut = boost::to_lower_copy(chainOutNames.back());
            
            if (lastChainOut != "" && lastChainOut != verusChainName)
            {
                chainOutNames.push_back(verusChainName);
            }
            else if (lastChainOut == "")
            {
                chainOutNames.pop_back();
            }
        }

        std::string lastRetName = boost::to_lower_copy(retNames.back());
        if (lastRetName != "" && lastRetName != verusChainName)
        {
            retNames.push_back(verusChainName);
        }
        else if (lastRetName == "")
        {
            retNames.pop_back();
        }
    }

    for (int i = 0; i < retNames.size(); i++)
    {
        if (retNames[i].size() > KOMODO_ASSETCHAIN_MAXLEN - 1)
        {
            retNames[i] = std::string(retNames[i], 0, (KOMODO_ASSETCHAIN_MAXLEN - 1));
        }
        // spaces are allowed, but no sub-name can have leading or trailing spaces
        if (!retNames[i].size() || retNames[i] != TrimTrailing(TrimLeading(retNames[i], ' '), ' '))
        {
            return std::vector<std::string>();
        }
    }

    // if no explicit chain is specified, default to chain of the ID
    if (!explicitChain && retNames.size())
    {
        if (retNames.size() == 1 && retNames.back() != verusChainName)
        {
            // we are referring to an external root blockchain
            ChainOut = retNames[0];
        }
        else
        {
            for (int i = 1; i < retNames.size(); i++)
            {
                if (ChainOut.size())
                {
                    ChainOut = ChainOut + ".";
                }
                ChainOut = ChainOut + retNames[i];
            }
        }
    }

    return retNames;
}

// takes a multipart name, either complete or partially processed with a Parent hash,
// hash its parent names into a parent ID and return the parent hash and cleaned, single name
std::string CVDXF::CleanName(const std::string &Name, uint160 &Parent, bool displayfilter)
{
    std::string chainName;
    std::vector<std::string> subNames = ParseSubNames(Name, chainName);

    if (!subNames.size())
    {
        return "";
    }

    if (!Parent.IsNull() &&
        boost::to_lower_copy(subNames.back()) == boost::to_lower_copy(VERUS_CHAINNAME))
    {
        subNames.pop_back();
    }

    for (int i = subNames.size() - 1; i > 0; i--)
    {
        std::string parentNameStr = boost::algorithm::to_lower_copy(subNames[i]);
        const char *parentName = parentNameStr.c_str();
        uint256 idHash;

        if (Parent.IsNull())
        {
            idHash = Hash(parentName, parentName + parentNameStr.size());
        }
        else
        {
            idHash = Hash(parentName, parentName + strlen(parentName));
            idHash = Hash(Parent.begin(), Parent.end(), idHash.begin(), idHash.end());
        }
        Parent = Hash160(idHash.begin(), idHash.end());
        //printf("uint160 for parent %s: %s\n", parentName, Parent.GetHex().c_str());
    }
    return subNames[0];
}

uint160 CVDXF::GetID(const std::string &Name)
{
    uint160 parent;
    std::string cleanName = CleanName(Name, parent);

    std::string subName = boost::algorithm::to_lower_copy(cleanName);
    const char *idName = subName.c_str();
    //printf("hashing: %s, %s\n", idName, parent.GetHex().c_str());

    uint256 idHash;
    if (parent.IsNull())
    {
        idHash = Hash(idName, idName + strlen(idName));
    }
    else
    {
        idHash = Hash(idName, idName + strlen(idName));
        idHash = Hash(parent.begin(), parent.end(), idHash.begin(), idHash.end());

    }
    return Hash160(idHash.begin(), idHash.end());
}

uint160 CVDXF::GetID(const std::string &Name, uint160 &parent)
{
    std::string cleanName = CleanName(Name, parent);

    std::string subName = boost::algorithm::to_lower_copy(cleanName);
    const char *idName = subName.c_str();
    //printf("hashing: %s, %s\n", idName, parent.GetHex().c_str());

    uint256 idHash;
    if (parent.IsNull())
    {
        idHash = Hash(idName, idName + strlen(idName));
    }
    else
    {
        idHash = Hash(idName, idName + strlen(idName));
        idHash = Hash(parent.begin(), parent.end(), idHash.begin(), idHash.end());
    }
    return Hash160(idHash.begin(), idHash.end());
}

// calculate the data key for a name inside of a namespace
// if the namespace is null, use VERUS_CHAINID
uint160 CVDXF::GetDataKey(const std::string &keyName, uint160 nameSpaceID)
{
    if (nameSpaceID.IsNull())
    {
        nameSpaceID = VERUS_CHAINID;
    }
    uint160 parent = GetID(DATA_KEY_SEPARATOR, nameSpaceID);
    return GetID(keyName, parent);
}

bool uni_get_bool(UniValue uv, bool def)
{
    try
    {
        if (uv.isStr())
        {
            std::string boolStr;
            if ((boolStr = uni_get_str(uv, def ? "true" : "false")) == "true" || boolStr == "1")
            {
                return true;
            }
            else if (boolStr == "false" || boolStr == "0")
            {
                return false;
            }
            return def;
        }
        else if (uv.isNum())
        {
            return uv.get_int() != 0;
        }
        else
        {
            return uv.get_bool();
        }
        return false;
    }
    catch(const std::exception& e)
    {
        return def;
    }
}

int32_t uni_get_int(UniValue uv, int32_t def)
{
    try
    {
        return uv.get_int();
    }
    catch(const std::exception& e)
    {
        return def;
    }
}

int64_t uni_get_int64(UniValue uv, int64_t def)
{
    try
    {
        return uv.get_int64();
    }
    catch(const std::exception& e)
    {
        return def;
    }
}

std::string uni_get_str(UniValue uv, std::string def)
{
    try
    {
        return uv.get_str();
    }
    catch(const std::exception& e)
    {
        return def;
    }
}

std::vector<UniValue> uni_getValues(UniValue uv, std::vector<UniValue> def)
{
    try
    {
        return uv.getValues();
    }
    catch(const std::exception& e)
    {
        return def;
    }
}

