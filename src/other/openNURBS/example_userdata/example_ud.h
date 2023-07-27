#if !defined(OPENNURBS_EXAMPLE_UD_INC_)
#define OPENNURBS_EXAMPLE_UD_INC_

class CExampleWriteUserData : public ON_UserData
{
  static int m__sn;
  ON_OBJECT_DECLARE(CExampleWriteUserData);

public:
  static ON_UUID Id();

  CExampleWriteUserData();
  virtual ~CExampleWriteUserData();

  CExampleWriteUserData( const char* s);
  CExampleWriteUserData(const CExampleWriteUserData& src);
  CExampleWriteUserData& operator=(const CExampleWriteUserData& src);

  void Dump( ON_TextLog& text_log ) const override;
  bool GetDescription( ON_wString& description ) override;
  bool Archive() const override;
  bool Write(ON_BinaryArchive& file) const override;
  bool Read(ON_BinaryArchive& file) override;

  ON_wString m_str;
  int m_sn;
};

#endif
