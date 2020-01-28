#ifndef FILEZILLA_INTERFACE_SITEMANAGER_CONTROLS_HEADER
#define FILEZILLA_INTERFACE_SITEMANAGER_CONTROLS_HEADER

#include "serverdata.h"

class DialogLayout;
class Site;
enum ServerProtocol;
enum LogonType;

class SiteControls
{
public:
	SiteControls(wxWindow & parent)
	    : parent_(parent)
	{}

	virtual ~SiteControls() = default;
	virtual void SetSite(Site const& site, bool predefined) = 0;

	virtual bool Verify(bool /*predefined*/) { return true; }
	virtual void SetControlVisibility(ServerProtocol /*protocol*/, LogonType /*type*/) {}

	virtual void UpdateSite(Site &site) = 0;

	wxWindow & parent_;
};

class AdvancedSiteControls final : public SiteControls
{
public:
	AdvancedSiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer);

	virtual void SetSite(Site const& site, bool predefined) override;
	virtual bool Verify(bool predefined) override;
	virtual void SetControlVisibility(ServerProtocol protocol, LogonType) override;
	virtual void UpdateSite(Site & site) override;
};

class CharsetSiteControls final : public SiteControls
{
public:
	CharsetSiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer);

	virtual void SetSite(Site const& site, bool predefined) override;
	virtual bool Verify(bool predefined) override;
	virtual void UpdateSite(Site & site) override;
};

class TransferSettingsSiteControls final : public SiteControls
{
public:
	TransferSettingsSiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer);

	virtual void SetSite(Site const& site, bool predefined) override;
	virtual void SetControlVisibility(ServerProtocol protocol, LogonType) override;
	virtual void UpdateSite(Site & site) override;
};


class S3SiteControls final : public SiteControls
{
public:
	S3SiteControls(wxWindow & parent, DialogLayout const& lay, wxFlexGridSizer & sizer);

	virtual void SetSite(Site const& site, bool predefined) override;
	virtual void UpdateSite(Site & site) override;
};

#endif
