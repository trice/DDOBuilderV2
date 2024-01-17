// BonusTypesFile.cpp
//
#include "StdAfx.h"
#include "BonusTypesFile.h"
#include "XmlLib\SaxReader.h"

namespace
{
    const XmlLib::SaxString f_saxElementName = L"BonusTypes"; // root element name to look for
}

BonusTypesFile::BonusTypesFile(const std::string& filename) :
    SaxContentElement(f_saxElementName),
    m_filename(filename),
    m_loadTotal(0)
{
}


BonusTypesFile::~BonusTypesFile(void)
{
}

void BonusTypesFile::Read()
{
    // set up a reader with this as the expected root node
    XmlLib::SaxReader reader(this, f_saxElementName);
    // read in the xml from a file (fully qualified path)
    bool ok = reader.Open(m_filename);
    if (!ok)
    {
        std::string errorMessage = reader.ErrorMessage();
        // document has failed to load. Tell the user what we can about it
        CString text;
        text.Format("The document %s\n"
                "failed to load. The XML parser reported the following problem:\n"
                "\n", m_filename.c_str());
        text += errorMessage.c_str();
        AfxMessageBox(text, MB_ICONERROR);
    }
}

XmlLib::SaxContentElementInterface * BonusTypesFile::StartElement(
        const XmlLib::SaxString & name,
        const XmlLib::SaxAttributes & attributes)
{
    XmlLib::SaxContentElementInterface * subHandler =
            SaxContentElement::StartElement(name, attributes);
    if (subHandler == NULL)
    {
        Bonus bonus;
        if (bonus.SaxElementIsSelf(name, attributes))
        {
            m_loadedBonusTypes.push_back(bonus);
            subHandler = &(m_loadedBonusTypes.back());
        }
    }

    return subHandler;
}

void BonusTypesFile::EndElement()
{
    SaxContentElement::EndElement();
}

const std::list<Bonus> & BonusTypesFile::BonusTypes() const
{
    return m_loadedBonusTypes;
}
