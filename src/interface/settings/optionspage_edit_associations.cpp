#include <filezilla.h>

#include "../file_utils.h"
#include "../Options.h"
#include "../textctrlex.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_edit_associations.h"

bool COptionsPageEditAssociations::CreateControls(wxWindow* parent)
{
	auto const& lay = m_pOwner->layout();

	Create(parent);
	auto main = lay.createFlex(1);
	main->AddGrowableCol(0);
	main->AddGrowableRow(1);
	SetSizer(main);

	main->Add(new wxStaticText(this, -1, _("C&ustom filetype associations:")));

	assocs_ = new wxTextCtrlEx(this, -1, wxString(), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	main->Add(assocs_, lay.grow);

	main->Add(new wxStaticText(this, -1, _("Format: Extension followed by properly quoted command and arguments.")));

	main->Add(new wxStaticText(this, -1, _("Example: png \"c:\\program files\\viewer\\viewer.exe\" -open")));

	return true;
}

bool COptionsPageEditAssociations::LoadPage()
{
	assocs_->ChangeValue(m_pOptions->GetOption(OPTION_EDIT_CUSTOMASSOCIATIONS));
	return true;
}

bool COptionsPageEditAssociations::SavePage()
{
	m_pOptions->SetOption(OPTION_EDIT_CUSTOMASSOCIATIONS, assocs_->GetValue().ToStdWstring());
	return true;
}

bool COptionsPageEditAssociations::Validate()
{
	std::wstring const raw_assocs = assocs_->GetValue().ToStdWstring();
	auto assocs = fz::strtok(raw_assocs, L"\r\n", true);

	for (auto& assoc : assocs) {
		std::wstring command;
		if (!UnquoteCommand(assoc, command)) {
			return DisplayError(assocs_, _("Improperly quoted association."));
		}

		if (assoc.empty()) {
			return DisplayError(assocs_, _("Empty file extension."));
		}

		std::wstring args;
		if (!UnquoteCommand(command, args)) {
			return DisplayError(assocs_, _("Improperly quoted association."));
		}

		if (command.empty()) {
			return DisplayError(assocs_, _("Empty command."));
		}

		if (!ProgramExists(command)) {
			wxString error = _("Associated program not found:");
			error += '\n';
			error += command;
			return DisplayError(assocs_, error);
		}
	}

	return true;
}
